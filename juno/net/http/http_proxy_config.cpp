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
}

HttpProxyConfig::~HttpProxyConfig() {
}

HttpProxyConfig& HttpProxyConfig::operator=(const HttpProxyConfig& other) {
  if (&other != this) {
    use_remote_proxy_ = other.use_remote_proxy_;
    remote_proxy_host_ = other.remote_proxy_host_;
    remote_proxy_port_ = other.remote_proxy_port_;
    auth_remote_proxy_ = other.auth_remote_proxy_;
    remote_proxy_user_ = other.remote_proxy_user_;
    remote_proxy_password_ = other.remote_proxy_password_;
    header_filters_ = other.header_filters_;
  }

  return *this;
}

bool HttpProxyConfig::Load(const RegistryKey& key) {
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
  if (key.QueryBinary(kRemoteProxyPassword, nullptr, &length)) {
    BYTE* buffer = new BYTE[length];
    key.QueryBinary(kRemoteProxyPassword, buffer, &length);

    DATA_BLOB encrypted = { length, buffer }, decrypted = {};
    if (::CryptUnprotectData(&encrypted, nullptr, nullptr, nullptr, nullptr, 0,
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

  return true;
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

    if (::CryptProtectData(&decrypted, nullptr, nullptr, nullptr, nullptr, 0,
                           &encrypted)) {
      key->SetBinary(kRemoteProxyPassword, encrypted.pbData, encrypted.cbData);
      ::LocalFree(encrypted.pbData);
    }
  }

  key->DeleteKey(kHeaderFilters);

  RegistryKey filters_key;
  filters_key.Create(*key, kHeaderFilters);
  int index = 0;

  char name[16];

  for (auto& filter : header_filters_) {
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
