// copyright (c) 2014 dacci.org

#include "service/http/http_digest.h"

#include <windows.h>

#include <wincrypt.h>

#include <memory>
#include <string>

namespace {

class CryptHash {
 public:
  ~CryptHash() {
    Destroy();
  }

  bool Hash(const void* data, size_t length) {
    if (handle_ == NULL)
      return false;

    return CryptHashData(handle_, static_cast<const BYTE*>(data),
                         static_cast<DWORD>(length), 0) != FALSE;
  }

  bool Hash(const char* string) {
    return Hash(string, strlen(string));
  }

  bool Hash(const std::string& string) {
    return Hash(string.c_str(), string.size());
  }

  template <typename T>
  bool Hash(const T& data) {
    return Hash(&data, sizeof(data));
  }

  const std::string& Complete() {
    BOOL succeeded;

    do {
      if (handle_ == NULL)
        break;

      DWORD hash_length = 0;
      DWORD length = sizeof(hash_length);
      succeeded =
          CryptGetHashParam(handle_, HP_HASHSIZE,
                            reinterpret_cast<BYTE*>(&hash_length), &length, 0);
      if (!succeeded)
        break;

      auto hash_data = std::make_unique<BYTE[]>(hash_length);
      if (hash_data == nullptr)
        break;

      succeeded = CryptGetHashParam(handle_, HP_HASHVAL, hash_data.get(),
                                    &hash_length, 0);
      if (!succeeded)
        break;

      hash_.resize(hash_length * 2);
      length = static_cast<DWORD>(hash_.capacity());

      succeeded = CryptBinaryToStringA(
          hash_data.get(), hash_length,
          CRYPT_STRING_HEXRAW | CRYPT_STRING_NOCRLF, &hash_[0], &length);
      if (!succeeded) {
        hash_.clear();
        break;
      }

      Destroy();
    } while (false);

    return hash_;
  }

 private:
  friend class CryptProvider;

  explicit CryptHash(HCRYPTHASH handle) : handle_(handle) {}

  void Destroy() {
    if (handle_ != NULL) {
      CryptDestroyHash(handle_);
      handle_ = NULL;
    }
  }

  HCRYPTHASH handle_;
  std::string hash_;

  CryptHash(const CryptHash&) = delete;
  CryptHash& operator=(const CryptHash&) = delete;
};

class CryptProvider {
 public:
  CryptProvider() : handle_(NULL) {
    CryptAcquireContext(&handle_, nullptr, nullptr, PROV_RSA_FULL,
                        CRYPT_VERIFYCONTEXT);
  }

  ~CryptProvider() {
    if (IsValid()) {
      CryptReleaseContext(handle_, 0);
      handle_ = NULL;
    }
  }

  std::unique_ptr<CryptHash> CreateHash(ALG_ID algorithm) {
    struct Wrapper : CryptHash {
      explicit Wrapper(HCRYPTHASH handle) : CryptHash(handle) {}
    };

    HCRYPTHASH hash = NULL;
    if (CryptCreateHash(handle_, algorithm, NULL, 0, &hash))
      return std::make_unique<Wrapper>(hash);
    else
      return nullptr;
  }

  std::unique_ptr<BYTE[]> GenerateRandom(DWORD length) {
    auto random = std::make_unique<BYTE[]>(length);
    if (random) {
      auto succeeded = CryptGenRandom(handle_, length, random.get());
      if (!succeeded)
        random.reset(nullptr);
    }

    return random;
  }

  bool IsValid() const {
    return handle_ != NULL;
  }

 private:
  HCRYPTPROV handle_;
};

bool GetPair(const char* input, std::string* name, std::string* value,
             const char** endptr) {
  auto str = input;
  auto starts_with_quote = false;
  auto escape = false;

  while (*str && *str != '=')
    ++str;
  if (*str != '=')
    return false;

  name->assign(input, str);
  ++str;

  if (*str == '"') {
    ++str;
    starts_with_quote = true;
  }

  for (auto loop = true; loop && *str; ++str) {
    switch (*str) {
      case '\\':
        if (!escape) {
          escape = true;
          value->push_back('\\');
          continue;
        }
        break;

      case ',':
        if (!starts_with_quote) {
          loop = false;
          continue;
        }
        break;

      case '\r':
      case '\n':
        loop = false;
        continue;

      case '\"':
        if (!escape && starts_with_quote) {
          loop = false;
          continue;
        }
        break;
    }

    escape = false;
    value->push_back(*str);
  }

  *endptr = str;

  return true;
}

}  // namespace

static CryptProvider crypt_provider;

HttpDigest::HttpDigest() {
  Reset();
}

bool HttpDigest::Input(const std::string& input) {
  auto header = input.data();
  if (strncmp(header, "Digest", 6) == 0)
    header += 6;
  else
    return false;

  Reset();

  for (;;) {
    while (*header && isspace(*header))
      ++header;

    std::string name;
    std::string value;
    if (!GetPair(header, &name, &value, &header))
      break;

    if (name.compare("nonce") == 0) {
      nonce_ = std::move(value);
    } else if (name.compare("stale") == 0) {
      if (value.compare("true") == 0) {
        stale_ = true;
        nonce_count_ = 1;
      }
    } else if (name.compare("realm") == 0) {
      realm_ = std::move(value);
    } else if (name.compare("opaque") == 0) {
      opaque_ = std::move(value);
    } else if (name.compare("qop") == 0) {
      auto auth_found = false, auth_int_found = false;

      char* tok_buf = nullptr;
      auto token = strtok_s(&value[0], ",", &tok_buf);
      while (token != nullptr) {
        if (strcmp(token, "auth") == 0)
          auth_found = true;
        else if (strcmp(token, "auth-int") == 0)
          auth_int_found = true;

        token = strtok_s(nullptr, ",", &tok_buf);
      }

      if (auth_found)
        qop_ = "auth";
      else if (auth_int_found)
        qop_ = "auth-int";
    } else if (name.compare("algorithm") == 0) {
      if (value.compare("MD5-sess") == 0)
        digest_ = MD5SESS;
      else if (value.compare("MD5") == 0)
        digest_ = MD5;
      else
        return false;

      algorithm_ = std::move(value);
    }

    while (*header && isspace(*header))
      ++header;
    if (*header == ',')
      ++header;
  }

  return true;
}

bool HttpDigest::Output(const std::string& method, const std::string& path,
                        std::string* output) {
  if (nonce_count_ == 0)
    nonce_count_ = 1;

  if (cnonce_.empty()) {
    SYSTEMTIME system_time;
    FILETIME file_time;
    GetSystemTime(&system_time);
    SystemTimeToFileTime(&system_time, &file_time);

    auto random1 = crypt_provider.GenerateRandom(4);
    auto random2 = crypt_provider.GenerateRandom(4);

    std::string cnonce_buffer;
    cnonce_buffer.resize(32);
    sprintf_s(&cnonce_buffer[0], cnonce_buffer.capacity(), "%08x%08x%08x%08x",
              *reinterpret_cast<DWORD*>(random1.get()),
              *reinterpret_cast<DWORD*>(random2.get()),
              file_time.dwHighDateTime, file_time.dwLowDateTime);

    cnonce_.resize(44);
    auto length = static_cast<DWORD>(cnonce_.capacity());
    CryptBinaryToStringA(reinterpret_cast<const BYTE*>(cnonce_buffer.data()),
                         static_cast<DWORD>(cnonce_buffer.size()),
                         CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &cnonce_[0],
                         &length);
    cnonce_.resize(length);
  }

  std::string ha1;
  {
    auto hash = crypt_provider.CreateHash(CALG_MD5);
    hash->Hash(username_);
    hash->Hash(":");
    hash->Hash(realm_);
    hash->Hash(":");
    hash->Hash(password_);
    ha1 = hash->Complete();
  }

  if (digest_ == MD5SESS) {
    auto hash = crypt_provider.CreateHash(CALG_MD5);
    hash->Hash(ha1);
    hash->Hash(":");
    hash->Hash(nonce_);
    hash->Hash(":");
    hash->Hash(cnonce_);
    ha1 = hash->Complete();
  }

  std::string ha2;
  {
    auto hash = crypt_provider.CreateHash(CALG_MD5);
    hash->Hash(method);
    hash->Hash(":");
    hash->Hash(path);

    if (qop_.compare("auth-int") == 0) {
      hash->Hash(":");
      hash->Hash("d41d8cd98f00b204e9800998ecf8427e");  // MD5 hash of empty
    }

    ha2 = hash->Complete();
  }

  auto hash = crypt_provider.CreateHash(CALG_MD5);
  hash->Hash(ha1);
  hash->Hash(":");
  hash->Hash(nonce_);

  if (!qop_.empty()) {
    char nc[9];
    sprintf_s(nc, "%08x", nonce_count_);

    hash->Hash(":");
    hash->Hash(nc);
    hash->Hash(":");
    hash->Hash(cnonce_);
    hash->Hash(":");
    hash->Hash(qop_);
  }

  hash->Hash(":");
  hash->Hash(ha2);

  output->assign("Digest ")
      .append("username=\"")
      .append(username_)
      .append("\", realm=\"")
      .append(realm_)
      .append("\", nonce=\"")
      .append(nonce_)
      .append("\", uri=\"")
      .append(path);

  if (!qop_.empty()) {
    char nc[9];
    sprintf_s(nc, "%08x", nonce_count_);

    output->append("\", cnonce=\"")
        .append(cnonce_)
        .append("\", nc=\"")
        .append(nc)
        .append("\", qop=\"")
        .append(qop_);

    if (qop_.compare("auth") == 0)
      ++nonce_count_;
  }

  output->append("\", response=\"").append(hash->Complete());

  if (!opaque_.empty())
    output->append("\", opaque=\"").append(opaque_);

  if (!algorithm_.empty())
    output->append("\", algorithm=\"").append(algorithm_);

  output->append("\"");

  return true;
}

void HttpDigest::SetCredential(const std::string& username,
                               const std::string& password) {
  username_ = username;
  password_ = password;

  Reset();
}

void HttpDigest::Reset() {
  nonce_count_ = 0;
  digest_ = MD5;
  stale_ = false;

  nonce_.clear();
  cnonce_.clear();
  realm_.clear();
  opaque_.clear();
  qop_.clear();
  algorithm_.clear();
}
