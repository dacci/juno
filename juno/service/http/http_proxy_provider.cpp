// Copyright (c) 2014 dacci.org

#include "service/http/http_proxy_provider.h"

#include <windows.h>

#include <wincrypt.h>

#include <base/values.h>

#include <memory>
#include <string>

#include "misc/registry_key-inl.h"
#include "service/http/http_proxy.h"
#include "service/http/http_proxy_config.h"
#include "ui/http_proxy_dialog.h"

namespace juno {
namespace service {
namespace http {
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

const std::string kUseRemoteProxyJson = "use_remote_proxy";
const std::string kRemoteProxyHostJson = "remote_proxy_host";
const std::string kRemoteProxyPortJson = "remote_proxy_port";
const std::string kAuthRemoteProxyJson = "auth_remote_proxy";
const std::string kRemoteProxyUserJson = "remote_proxy_user";
const std::string kRemoteProxyPasswordJson = "remote_proxy_password";
const std::string kHeaderFiltersJson = "header_filters";
const std::string kRequestJson = "request";
const std::string kResponseJson = "response";
const std::string kActionJson = "action";
const std::string kNameJson = "name";
const std::string kValueJson = "value";
const std::string kReplaceJson = "replace";

}  // namespace

std::unique_ptr<ServiceConfig> HttpProxyProvider::CreateConfig() {
  return std::make_unique<HttpProxyConfig>();
}

std::unique_ptr<ServiceConfig> HttpProxyProvider::LoadConfig(
    const misc::RegistryKey& key) {
  auto config = std::make_unique<HttpProxyConfig>();
  if (config == nullptr)
    return nullptr;

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
    auto buffer = std::make_unique<BYTE[]>(length);
    key.QueryBinary(kRemoteProxyPassword, buffer.get(), &length);

    DATA_BLOB encrypted{static_cast<DWORD>(length), buffer.get()}, decrypted{};
    if (CryptUnprotectData(&encrypted, nullptr, nullptr, nullptr, nullptr, 0,
                           &decrypted)) {
      config->remote_proxy_password_.assign(
          reinterpret_cast<char*>(decrypted.pbData), decrypted.cbData);
      LocalFree(decrypted.pbData);
    }
  }

  misc::RegistryKey filters_key;
  if (filters_key.Open(key, kHeaderFilters)) {
    for (auto i = 0;; ++i) {
      std::string name;
      if (!filters_key.EnumerateKey(i, &name))
        break;

      misc::RegistryKey filter_key;
      filter_key.Open(filters_key, name);

      config->header_filters_.push_back(HttpProxyConfig::HeaderFilter());
      auto& filter = config->header_filters_.back();

      filter.request = filter_key.QueryInteger(kRequest) != 0;
      filter.response = filter_key.QueryInteger(kResponse) != 0;
      filter.action = static_cast<HttpProxyConfig::FilterAction>(
          filter_key.QueryInteger(kAction));
      filter_key.QueryString(kName, &filter.name);
      filter_key.QueryString(kValue, &filter.value);
      filter_key.QueryString(kReplace, &filter.replace);
    }

    filters_key.Close();
  }

  return std::move(config);
}

bool HttpProxyProvider::SaveConfig(const ServiceConfig* base_config,
                                   misc::RegistryKey* key) {
  if (base_config == nullptr || key == nullptr)
    return false;

  auto config = static_cast<const HttpProxyConfig*>(base_config);

  key->SetInteger(kUseRemoteProxy, config->use_remote_proxy_);
  key->SetString(kRemoteProxyHost, config->remote_proxy_host_);
  key->SetInteger(kRemoteProxyPort, config->remote_proxy_port_);
  key->SetInteger(kAuthRemoteProxy, config->auth_remote_proxy_);
  key->SetString(kRemoteProxyUser, config->remote_proxy_user_);

  if (!config->remote_proxy_password_.empty()) {
    auto password = config->remote_proxy_password_;
    DATA_BLOB decrypted{static_cast<DWORD>(password.size()),
                        reinterpret_cast<BYTE*>(&password[0])};
    DATA_BLOB encrypted{};

    if (CryptProtectData(&decrypted, nullptr, nullptr, nullptr, nullptr, 0,
                         &encrypted)) {
      key->SetBinary(kRemoteProxyPassword, encrypted.pbData, encrypted.cbData);
      LocalFree(encrypted.pbData);
    }
  }

  key->DeleteKey(kHeaderFilters);

  misc::RegistryKey filters_key;
  filters_key.Create(*key, kHeaderFilters);
  auto index = 0;

  char name[16];

  for (auto& filter : config->header_filters_) {
    sprintf_s(name, "%d", index++);

    misc::RegistryKey filter_key;
    filter_key.Create(filters_key, name);

    filter_key.SetInteger(kRequest, filter.request);
    filter_key.SetInteger(kResponse, filter.response);
    filter_key.SetInteger(kAction, static_cast<int>(filter.action));
    filter_key.SetString(kName, filter.name);
    filter_key.SetString(kValue, filter.value);
    filter_key.SetString(kReplace, filter.replace);
  }

  return true;
}

std::unique_ptr<ServiceConfig> HttpProxyProvider::CopyConfig(
    const ServiceConfig* base_config) {
  if (base_config == nullptr)
    return nullptr;

  return std::make_unique<HttpProxyConfig>(
      *static_cast<const HttpProxyConfig*>(base_config));
}

std::unique_ptr<base::DictionaryValue> HttpProxyProvider::ConvertConfig(
    const ServiceConfig* base_config) {
  if (base_config == nullptr)
    return nullptr;

  auto value = std::make_unique<base::DictionaryValue>();
  if (value == nullptr)
    return nullptr;

  auto config = static_cast<const HttpProxyConfig*>(base_config);

  auto filters = std::make_unique<base::ListValue>();
  if (filters == nullptr)
    return nullptr;

  for (const auto& header_filter : config->header_filters_) {
    auto filter = std::make_unique<base::DictionaryValue>();
    if (filter == nullptr)
      return nullptr;

    filter->SetBoolean(kRequestJson, header_filter.request);
    filter->SetBoolean(kResponseJson, header_filter.response);
    filter->SetInteger(kActionJson, static_cast<int>(header_filter.action));
    filter->SetString(kNameJson, header_filter.name);
    filter->SetString(kValueJson, header_filter.value);
    filter->SetString(kReplaceJson, header_filter.replace);

    filters->Append(std::move(filter));
  }

  value->SetBoolean(kUseRemoteProxyJson, config->use_remote_proxy_);
  value->SetString(kRemoteProxyHostJson, config->remote_proxy_host_);
  value->SetInteger(kRemoteProxyPortJson, config->remote_proxy_port_);
  value->SetBoolean(kAuthRemoteProxyJson, config->auth_remote_proxy_);
  value->SetString(kRemoteProxyUserJson, config->remote_proxy_user_);
  value->SetString(kRemoteProxyPasswordJson, config->remote_proxy_password_);
  value->Set(kHeaderFiltersJson, std::move(filters));

  return std::move(value);
}

std::unique_ptr<ServiceConfig> HttpProxyProvider::ConvertConfig(
    const base::DictionaryValue* value) {
  if (value == nullptr)
    return nullptr;

  auto config = std::make_unique<HttpProxyConfig>();
  if (config == nullptr)
    return nullptr;

  value->GetBoolean(kUseRemoteProxyJson, &config->use_remote_proxy_);
  value->GetString(kRemoteProxyHostJson, &config->remote_proxy_host_);
  value->GetInteger(kRemoteProxyPortJson, &config->remote_proxy_port_);
  value->GetBoolean(kAuthRemoteProxyJson, &config->auth_remote_proxy_);
  value->GetString(kRemoteProxyUserJson, &config->remote_proxy_user_);
  value->GetString(kRemoteProxyPasswordJson, &config->remote_proxy_password_);

  const base::ListValue* filters;
  if (value->GetList(kHeaderFiltersJson, &filters)) {
    for (const auto& item : *filters) {
      const base::DictionaryValue* filter;
      if (!item->GetAsDictionary(&filter))
        continue;

      HttpProxyConfig::HeaderFilter header_filter{};
      int action;

      filter->GetBoolean(kRequestJson, &header_filter.request);
      filter->GetBoolean(kResponseJson, &header_filter.response);
      if (filter->GetInteger(kActionJson, &action))
        header_filter.action =
            static_cast<HttpProxyConfig::FilterAction>(action);
      filter->GetString(kNameJson, &header_filter.name);
      filter->GetString(kValueJson, &header_filter.value);

      filter->GetString(kReplaceJson, &header_filter.replace);
      config->header_filters_.push_back(std::move(header_filter));
    }
  }

  return std::move(config);
}

std::unique_ptr<Service> HttpProxyProvider::CreateService(
    const ServiceConfig* base_config) {
  if (base_config == nullptr)
    return nullptr;

  auto service = std::make_unique<HttpProxy>();
  if (service == nullptr)
    return nullptr;

  if (!service->UpdateConfig(base_config))
    return nullptr;

  return std::move(service);
}

INT_PTR HttpProxyProvider::Configure(ServiceConfig* base_config, HWND parent) {
  return ui::HttpProxyDialog(static_cast<HttpProxyConfig*>(base_config))
      .DoModal(parent);
}

}  // namespace http
}  // namespace service
}  // namespace juno
