// Copyright (c) 2014 dacci.org

#include "service/http/http_proxy_config.h"

#include <windows.h>
#include <wincrypt.h>

#include <string>

#include "misc/registry_key-inl.h"
#include "service/http/http_request.h"
#include "service/http/http_response.h"

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

const std::string kProxyAuthenticate("Proxy-Authenticate");
const std::string kProxyAuthorization("Proxy-Authorization");
}  // namespace

HttpProxyConfig::HttpProxyConfig()
    : use_remote_proxy_(0),
      remote_proxy_port_(0),
      auth_remote_proxy_(0),
      auth_digest_(false),
      auth_basic_(false) {
}

HttpProxyConfig::HttpProxyConfig(const HttpProxyConfig& other)
    : ServiceConfig(other),
      use_remote_proxy_(other.use_remote_proxy_),
      remote_proxy_host_(other.remote_proxy_host_),
      remote_proxy_port_(other.remote_proxy_port_),
      auth_remote_proxy_(other.auth_remote_proxy_),
      remote_proxy_user_(other.remote_proxy_user_),
      remote_proxy_password_(other.remote_proxy_password_),
      header_filters_(other.header_filters_),
      auth_digest_(false),
      auth_basic_(false) {
  SetCredential();
}

std::shared_ptr<HttpProxyConfig> HttpProxyConfig::Load(const RegistryKey& key) {
  auto config = std::make_shared<HttpProxyConfig>();
  if (config == nullptr)
    return config;

  int int_value;
  std::string string_value;

  if (key.QueryInteger(kUseRemoteProxy, &int_value))
    config->use_remote_proxy_ = int_value != 0;

  if (key.QueryString(kRemoteProxyHost, &string_value))
    config->remote_proxy_host_ = string_value;
  else
    config->use_remote_proxy_ = false;

  key.QueryInteger(kRemoteProxyPort, &int_value);
  if (0 < int_value && int_value < 65536)
    config->remote_proxy_port_ = int_value;
  else
    config->use_remote_proxy_ = false;

  if (key.QueryInteger(kAuthRemoteProxy, &int_value))
    config->auth_remote_proxy_ = int_value != 0;

  if (key.QueryString(kRemoteProxyUser, &string_value))
    config->remote_proxy_user_ = string_value;
  else
    config->auth_remote_proxy_ = false;

  int length;
  if (key.QueryBinary(kRemoteProxyPassword, nullptr, &length)) {
    auto buffer = new BYTE[length];
    key.QueryBinary(kRemoteProxyPassword, buffer, &length);

    DATA_BLOB encrypted{ static_cast<DWORD>(length), buffer }, decrypted{};
    if (CryptUnprotectData(&encrypted, nullptr, nullptr, nullptr, nullptr, 0,
                           &decrypted)) {
      config->remote_proxy_password_.assign(
          reinterpret_cast<char*>(decrypted.pbData), decrypted.cbData);
      LocalFree(decrypted.pbData);
    }

    delete[] buffer;
  }

  config->SetCredential();

  RegistryKey filters_key;
  if (filters_key.Open(key, kHeaderFilters)) {
    for (auto i = 0; ; ++i) {
      std::string name;
      if (!filters_key.EnumerateKey(i, &name))
        break;

      RegistryKey filter_key;
      filter_key.Open(filters_key, name);

      config->header_filters_.push_back(HeaderFilter());
      auto& filter = config->header_filters_.back();

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

  return config;
}

bool HttpProxyConfig::Save(RegistryKey* key) {
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

    if (CryptProtectData(&decrypted, nullptr, nullptr, nullptr, nullptr, 0,
                         &encrypted)) {
      key->SetBinary(kRemoteProxyPassword, encrypted.pbData, encrypted.cbData);
      LocalFree(encrypted.pbData);
    }
  }

  key->DeleteKey(kHeaderFilters);

  RegistryKey filters_key;
  filters_key.Create(*key, kHeaderFilters);
  int index = 0;

  char name[16];

  for (auto& filter : header_filters_) {
    sprintf_s(name, "%d", index++);

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

void HttpProxyConfig::FilterHeaders(HttpHeaders* headers, bool request) const {
  for (auto& filter : header_filters_) {
    if (request && filter.request || !request && filter.response) {
      switch (filter.action) {
        case Set:
          headers->SetHeader(filter.name, filter.value);
          break;

        case Append:
          headers->AppendHeader(filter.name, filter.value);
          break;

        case Add:
          headers->AddHeader(filter.name, filter.value);
          break;

        case Unset:
          headers->RemoveHeader(filter.name);
          break;

        case Merge:
          headers->MergeHeader(filter.name, filter.value);
          break;

        case Edit:
          headers->EditHeader(filter.name, filter.value, filter.replace, false);
          break;

        case EditR:
          headers->EditHeader(filter.name, filter.value, filter.replace, true);
          break;
      }
    }
  }
}

void HttpProxyConfig::ProcessAuthenticate(HttpResponse* response,
                                    HttpRequest* request) {
  base::AutoLock guard(lock_);

  DoProcessAuthenticate(response);
  DoProcessAuthorization(request);
}

void HttpProxyConfig::ProcessAuthorization(HttpRequest* request) {
  base::AutoLock guard(lock_);

  DoProcessAuthorization(request);
}

void HttpProxyConfig::SetCredential() {
  digest_.SetCredential(remote_proxy_user_, remote_proxy_password_);

  auto auth = remote_proxy_user_ + ':' + remote_proxy_password_;

  DWORD buffer_size = (((auth.size() - 1) / 3) + 1) * 4;
  basic_credential_.resize(buffer_size);
  buffer_size = basic_credential_.capacity();

  CryptBinaryToStringA(reinterpret_cast<const BYTE*>(auth.c_str()),
                       auth.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                       &basic_credential_[0], &buffer_size);

  basic_credential_.insert(0, "Basic ");
}

void HttpProxyConfig::DoProcessAuthenticate(HttpResponse* response) {
  if (!response->HeaderExists(kProxyAuthenticate))
    return;
  if (!auth_remote_proxy_)
    return;

  auth_digest_ = false;
  auth_basic_ = false;

  for (auto& field : response->GetAllHeaders(kProxyAuthenticate)) {
    if (strncmp(field.c_str(), "Digest", 6) == 0)
      auth_digest_ = digest_.Input(field);
    else if (strncmp(field.c_str(), "Basic", 5) == 0)
      auth_basic_ = true;
  }
}

void HttpProxyConfig::DoProcessAuthorization(HttpRequest* request) {
  // client's request has precedence
  if (request->HeaderExists(kProxyAuthorization))
    return;

  if (!auth_remote_proxy_)
    return;

  if (auth_digest_) {
    std::string field;
    if (digest_.Output(request->method(), request->path(), &field))
      request->SetHeader(kProxyAuthorization, field);
  } else if (auth_basic_) {
    request->SetHeader(kProxyAuthorization, basic_credential_);
  }
}
