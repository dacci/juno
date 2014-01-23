// Copyright (c) 2014 dacci.org

#include "net/http/http_proxy_config.h"

#include <wincrypt.h>

#include <madoka/concurrent/lock_guard.h>

#include "misc/registry_key-inl.h"

namespace {
const char kUseRemoteProxy[] = "UseRemoteProxy";
const char kRemoteProxyHost[] = "RemoteProxyHost";
const char kRemoteProxyPort[] = "RemoteProxyPort";
const char kAuthRemoteProxy[] = "AuthRemoteProxy";
const char kRemoteProxyUser[] = "RemoteProxyUser";
const char kRemoteProxyPassword[] = "RemoteProxyPassword";
const char kHeaderFilters[] = "HeaderFilters";
const char kRequest[] = "Request";
const char kResponse[] = "Response";
const char kAction[] = "Action";
const char kName[] = "Name";
const char kValue[] = "Value";
const char kReplace[] = "Replace";
}  // namespace

HttpProxyConfig::HttpProxyConfig()
    : use_remote_proxy_(), remote_proxy_port_(), auth_remote_proxy_() {
}

HttpProxyConfig::HttpProxyConfig(const HttpProxyConfig& other)
    : ServiceConfig(other),
      use_remote_proxy_(other.use_remote_proxy_),
      remote_proxy_host_(other.remote_proxy_host_),
      remote_proxy_port_(other.remote_proxy_port_),
      auth_remote_proxy_(other.auth_remote_proxy_),
      remote_proxy_user_(other.remote_proxy_user_),
      remote_proxy_password_(other.remote_proxy_password_),
      header_filters_(other.header_filters_) {
  SetBasicAuthorization();
}

HttpProxyConfig::~HttpProxyConfig() {
}

HttpProxyConfig& HttpProxyConfig::operator=(const HttpProxyConfig& other) {
  if (&other != this) {
    madoka::concurrent::WriteLock write_lock(&lock_);
    madoka::concurrent::LockGuard guard(&write_lock);

    use_remote_proxy_ = other.use_remote_proxy_;
    remote_proxy_host_ = other.remote_proxy_host_;
    remote_proxy_port_ = other.remote_proxy_port_;
    auth_remote_proxy_ = other.auth_remote_proxy_;
    remote_proxy_user_ = other.remote_proxy_user_;
    remote_proxy_password_ = other.remote_proxy_password_;
    header_filters_ = other.header_filters_;

    SetBasicAuthorization();
  }

  return *this;
}

bool HttpProxyConfig::Load(const RegistryKey& key) {
  madoka::concurrent::WriteLock write_lock(&lock_);
  madoka::concurrent::LockGuard guard(&write_lock);

  key.QueryInteger(kUseRemoteProxy, &use_remote_proxy_);

  if (!key.QueryString(kRemoteProxyHost, &remote_proxy_host_))
    use_remote_proxy_ = FALSE;

  key.QueryInteger(kRemoteProxyPort, &remote_proxy_port_);
  if (remote_proxy_port_ <= 0 || 65536 <= remote_proxy_port_)
    use_remote_proxy_ = FALSE;

  key.QueryInteger(kAuthRemoteProxy, &auth_remote_proxy_);

  if (!key.QueryString(kRemoteProxyUser, &remote_proxy_user_))
    auth_remote_proxy_ = 0;

  int length = 0;
  if (key.QueryBinary(kRemoteProxyPassword, NULL, &length)) {
    BYTE* buffer = new BYTE[length];
    key.QueryBinary(kRemoteProxyPassword, buffer, &length);

    DATA_BLOB encrypted = { length, buffer }, decrypted = {};
    if (::CryptUnprotectData(&encrypted, NULL, NULL, NULL, NULL, 0,
                             &decrypted)) {
      remote_proxy_password_.assign(reinterpret_cast<char*>(decrypted.pbData),
                                    decrypted.cbData);
      ::LocalFree(decrypted.pbData);
    }

    delete[] buffer;
  }

  RegistryKey filters_key;
  if (filters_key.Open(key, kHeaderFilters)) {
    for (int i = 0; ; ++i) {
      std::string name;
      if (!filters_key.EnumerateKey(i, &name))
        break;

      RegistryKey filter_key;
      filter_key.Open(filters_key, name);

      header_filters_.push_back(HeaderFilter());
      HeaderFilter& filter = header_filters_.back();

      filter.request = filter_key.QueryInteger(kRequest) != 0;
      filter.response = filter_key.QueryInteger(kResponse) != 0;
      filter.action =
          static_cast<FilterAction>(filter_key.QueryInteger(kAction));
      filter_key.QueryString(kName, &filter.name);
      filter_key.QueryString(kValue, &filter.value);
      filter_key.QueryString(kReplace, &filter.replace);
    }

    filters_key.Close();
  }

  SetBasicAuthorization();

  return true;
}

bool HttpProxyConfig::Save(RegistryKey* key) {
  madoka::concurrent::ReadLock read_lock(&lock_);
  madoka::concurrent::LockGuard guard(&read_lock);

  key->SetInteger(kUseRemoteProxy, use_remote_proxy_);
  key->SetString(kRemoteProxyHost, remote_proxy_host_);
  key->SetInteger(kRemoteProxyPort, remote_proxy_port_);
  key->SetInteger(kAuthRemoteProxy, auth_remote_proxy_);
  key->SetString(kRemoteProxyUser, remote_proxy_user_);

  if (!remote_proxy_password_.empty()) {
    DATA_BLOB decrypted = {
      remote_proxy_password_.size(),
      reinterpret_cast<BYTE*>(&remote_proxy_password_[0])
    };
    DATA_BLOB encrypted = {};

    if (::CryptProtectData(&decrypted, NULL, NULL, NULL, NULL, 0, &encrypted)) {
      key->SetBinary(kRemoteProxyPassword, encrypted.pbData, encrypted.cbData);
      ::LocalFree(encrypted.pbData);
    }
  }

  key->DeleteKey(kHeaderFilters);

  RegistryKey filters_key;
  filters_key.Create(*key, kHeaderFilters);
  int index = 0;

  char name[16];

  for (auto i = header_filters_.begin(), l = header_filters_.end(); i != l;
       ++i) {
    HeaderFilter& filter = *i;

    ::sprintf_s(name, "%d", index++);

    RegistryKey filter_key;
    filter_key.Create(filters_key, name);

    filter_key.SetInteger(kRequest, filter.request);
    filter_key.SetInteger(kResponse, filter.response);
    filter_key.SetInteger(kAction, filter.action);
    filter_key.SetString(kName, filter.name);
    filter_key.SetString(kValue, filter.value);
    filter_key.SetString(kReplace, filter.replace);
  }

  return true;
}

int HttpProxyConfig::use_remote_proxy() {
  madoka::concurrent::ReadLock read_lock(&lock_);
  madoka::concurrent::LockGuard guard(&read_lock);

  return use_remote_proxy_;
}

std::string HttpProxyConfig::remote_proxy_host() {
  madoka::concurrent::ReadLock read_lock(&lock_);
  madoka::concurrent::LockGuard guard(&read_lock);

  return remote_proxy_host_;
}

int HttpProxyConfig::remote_proxy_port() {
  madoka::concurrent::ReadLock read_lock(&lock_);
  madoka::concurrent::LockGuard guard(&read_lock);

  return remote_proxy_port_;
}

int HttpProxyConfig::auth_remote_proxy() {
  madoka::concurrent::ReadLock read_lock(&lock_);
  madoka::concurrent::LockGuard guard(&read_lock);

  return auth_remote_proxy_;
}

std::string HttpProxyConfig::remote_proxy_user() {
  madoka::concurrent::ReadLock read_lock(&lock_);
  madoka::concurrent::LockGuard guard(&read_lock);

  return remote_proxy_user_;
}

std::string HttpProxyConfig::remote_proxy_password() {
  madoka::concurrent::ReadLock read_lock(&lock_);
  madoka::concurrent::LockGuard guard(&read_lock);

  return remote_proxy_password_;
}

std::string HttpProxyConfig::basic_authorization() {
  madoka::concurrent::ReadLock read_lock(&lock_);
  madoka::concurrent::LockGuard guard(&read_lock);

  return basic_authorization_;
}

void HttpProxyConfig::SetBasicAuthorization() {
  std::string auth = remote_proxy_user_ + ':' + remote_proxy_password_;
  DWORD buffer_size = (((auth.size() - 1) / 3) + 2) * 4;
  char* buffer = new char[buffer_size];

  ::CryptBinaryToStringA(reinterpret_cast<const BYTE*>(auth.c_str()),
                         auth.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                         buffer, &buffer_size);

  basic_authorization_ = "Basic ";
  basic_authorization_.append(buffer);

  delete[] buffer;
}
