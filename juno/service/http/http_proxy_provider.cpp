// Copyright (c) 2014 dacci.org

#include "service/http/http_proxy_provider.h"

#include <windows.h>
#include <wincrypt.h>

#include <base/strings/string_number_conversions.h>
#include <base/strings/sys_string_conversions.h>

#include <string>

#include "service/http/http_proxy.h"
#include "service/http/http_proxy_config.h"
#include "service/http/ui/http_proxy_dialog.h"

namespace juno {
namespace service {
namespace http {
namespace {

const wchar_t kUseRemoteProxyReg[] = L"UseRemoteProxy";
const wchar_t kRemoteProxyHostReg[] = L"RemoteProxyHost";
const wchar_t kRemoteProxyPortReg[] = L"RemoteProxyPort";
const wchar_t kAuthRemoteProxyReg[] = L"AuthRemoteProxy";
const wchar_t kRemoteProxyUserReg[] = L"RemoteProxyUser";
const wchar_t kRemoteProxyPasswordReg[] = L"RemoteProxyPassword";
const wchar_t kHeaderFiltersReg[] = L"HeaderFilters";
const wchar_t kRequestReg[] = L"Request";
const wchar_t kResponseReg[] = L"Response";
const wchar_t kActionReg[] = L"Action";
const wchar_t kNameReg[] = L"Name";
const wchar_t kValueReg[] = L"Value";
const wchar_t kReplaceReg[] = L"Replace";

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

using ::base::win::RegKey;
using ::base::win::RegistryKeyIterator;

std::unique_ptr<ServiceConfig> HttpProxyProvider::CreateConfig() {
  return std::make_unique<HttpProxyConfig>();
}

std::unique_ptr<ServiceConfig> HttpProxyProvider::LoadConfig(
    const RegKey& key) {
  auto config = std::make_unique<HttpProxyConfig>();
  if (config == nullptr)
    return nullptr;

  DWORD int_value;
  std::wstring string_value;

  if (key.ReadValueDW(kUseRemoteProxyReg, &int_value) == ERROR_SUCCESS)
    config->use_remote_proxy_ = int_value != 0;

  if (key.ReadValue(kRemoteProxyHostReg, &string_value) == ERROR_SUCCESS)
    config->remote_proxy_host_ = base::SysWideToNativeMB(string_value);
  else
    config->use_remote_proxy_ = false;

  key.ReadValueDW(kRemoteProxyPortReg, &int_value);
  if (0 < int_value && int_value < 65536)
    config->remote_proxy_port_ = int_value;
  else
    config->use_remote_proxy_ = false;

  if (key.ReadValueDW(kAuthRemoteProxyReg, &int_value) == ERROR_SUCCESS)
    config->auth_remote_proxy_ = int_value != 0;

  if (key.ReadValue(kRemoteProxyUserReg, &string_value) == ERROR_SUCCESS)
    config->remote_proxy_user_ = base::SysWideToNativeMB(string_value);
  else
    config->auth_remote_proxy_ = false;

  DWORD length;
  if (key.ReadValue(kRemoteProxyPasswordReg, nullptr, &length, nullptr) ==
      ERROR_SUCCESS) {
    auto buffer = std::make_unique<BYTE[]>(length);
    key.ReadValue(kRemoteProxyPasswordReg, buffer.get(), &length, nullptr);

    DATA_BLOB encrypted{static_cast<DWORD>(length), buffer.get()}, decrypted{};
    if (CryptUnprotectData(&encrypted, nullptr, nullptr, nullptr, nullptr, 0,
                           &decrypted)) {
      config->remote_proxy_password_.assign(
          reinterpret_cast<char*>(decrypted.pbData), decrypted.cbData);
      LocalFree(decrypted.pbData);
    }
  }

  RegKey filters_key(key.Handle(), kHeaderFiltersReg, KEY_ENUMERATE_SUB_KEYS);
  if (filters_key.Valid()) {
    for (RegistryKeyIterator i(filters_key.Handle(), nullptr); i.Valid(); ++i) {
      RegKey filter_key(filters_key.Handle(), i.Name(), KEY_READ);
      if (!filter_key.Valid())
        continue;

      DWORD request, response, action;
      std::wstring name, value, replace;
      if (filter_key.ReadValueDW(kRequestReg, &request) != ERROR_SUCCESS ||
          filter_key.ReadValueDW(kResponseReg, &response) != ERROR_SUCCESS ||
          filter_key.ReadValueDW(kActionReg, &action) != ERROR_SUCCESS ||
          filter_key.ReadValue(kNameReg, &name) != ERROR_SUCCESS ||
          filter_key.ReadValue(kValueReg, &value) != ERROR_SUCCESS ||
          filter_key.ReadValue(kReplaceReg, &replace) != ERROR_SUCCESS)
        continue;

      HttpProxyConfig::HeaderFilter filter{};
      filter.request = request != 0;
      filter.response = response != 0;
      filter.action = static_cast<HttpProxyConfig::FilterAction>(action);
      filter.name = base::SysWideToUTF8(name);
      filter.value = base::SysWideToUTF8(value);
      filter.replace = base::SysWideToUTF8(replace);
      config->header_filters_.push_back(std::move(filter));
    }
  }

  return std::move(config);
}

bool HttpProxyProvider::SaveConfig(const ServiceConfig* base_config,
                                   RegKey* key) {
  if (base_config == nullptr || key == nullptr)
    return false;

  auto config = static_cast<const HttpProxyConfig*>(base_config);

  key->WriteValue(kUseRemoteProxyReg, config->use_remote_proxy_);
  key->WriteValue(kRemoteProxyHostReg,
                  base::SysNativeMBToWide(config->remote_proxy_host_).c_str());
  key->WriteValue(kRemoteProxyPortReg, config->remote_proxy_port_);
  key->WriteValue(kAuthRemoteProxyReg, config->auth_remote_proxy_);
  key->WriteValue(kRemoteProxyUserReg,
                  base::SysNativeMBToWide(config->remote_proxy_user_).c_str());

  if (!config->remote_proxy_password_.empty()) {
    auto password = config->remote_proxy_password_;
    DATA_BLOB decrypted{static_cast<DWORD>(password.size()),
                        reinterpret_cast<BYTE*>(&password[0])};
    DATA_BLOB encrypted{};

    if (CryptProtectData(&decrypted, nullptr, nullptr, nullptr, nullptr, 0,
                         &encrypted)) {
      key->WriteValue(kRemoteProxyPasswordReg, encrypted.pbData,
                      encrypted.cbData, REG_BINARY);
      LocalFree(encrypted.pbData);
    }
  }

  key->DeleteKey(kHeaderFiltersReg);

  RegKey filters_key(key->Handle(), kHeaderFiltersReg, KEY_ALL_ACCESS);
  auto index = 1;

  for (const auto& filter : config->header_filters_) {
    RegKey filter_key(filters_key.Handle(), base::IntToString16(index).c_str(),
                      KEY_ALL_ACCESS);
    if (!filter_key.Valid())
      continue;

    filter_key.WriteValue(kRequestReg, filter.request);
    filter_key.WriteValue(kResponseReg, filter.response);
    filter_key.WriteValue(kActionReg, static_cast<int>(filter.action));
    filter_key.WriteValue(kNameReg, base::SysUTF8ToWide(filter.name).c_str());
    filter_key.WriteValue(kValueReg, base::SysUTF8ToWide(filter.value).c_str());
    filter_key.WriteValue(kReplaceReg,
                          base::SysUTF8ToWide(filter.replace).c_str());

    ++index;
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
