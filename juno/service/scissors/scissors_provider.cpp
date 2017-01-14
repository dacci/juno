// Copyright (c) 2014 dacci.org

#include "service/scissors/scissors_provider.h"

#include <base/strings/sys_string_conversions.h>
#include <base/values.h>

#include <memory>
#include <string>

#include "service/scissors/scissors.h"
#include "service/scissors/scissors_config.h"
#include "ui/scissors_dialog.h"

namespace juno {
namespace service {
namespace scissors {
namespace {

const wchar_t kRemoteAddressReg[] = L"RemoteAddress";
const wchar_t kRemotePortReg[] = L"RemotePort";
const wchar_t kRemoteSSLReg[] = L"RemoteSSL";
const wchar_t kRemoteUDPReg[] = L"RemoteUDP";

const std::string kRemoteAddressJson = "remote_address";
const std::string kRemotePortJson = "remote_port";
const std::string kRemoteSSLJson = "remote_ssl";
const std::string kRemoteUDPJson = "remote_udp";

}  // namespace

std::unique_ptr<ServiceConfig> ScissorsProvider::CreateConfig() {
  return std::make_unique<ScissorsConfig>();
}

std::unique_ptr<ServiceConfig> ScissorsProvider::LoadConfig(
    const base::win::RegKey& key) {
  auto config = std::make_unique<ScissorsConfig>();
  if (config == nullptr)
    return nullptr;

  DWORD int_value;
  std::wstring string_value;

  if (key.ReadValue(kRemoteAddressReg, &string_value) != ERROR_SUCCESS ||
      key.ReadValueDW(kRemotePortReg, &int_value) != ERROR_SUCCESS)
    return nullptr;

  config->remote_address_ = base::SysWideToNativeMB(string_value);
  config->remote_port_ = int_value;

  if (key.ReadValueDW(kRemoteSSLReg, &int_value) == ERROR_SUCCESS)
    config->remote_ssl_ = int_value != 0;

  if (key.ReadValueDW(kRemoteUDPReg, &int_value) == ERROR_SUCCESS)
    config->remote_udp_ = int_value != 0;

  return std::move(config);
}

bool ScissorsProvider::SaveConfig(const ServiceConfig* base_config,
                                  base::win::RegKey* key) {
  if (base_config == nullptr || key == nullptr)
    return false;

  auto config = static_cast<const ScissorsConfig*>(base_config);

  key->WriteValue(kRemoteAddressReg,
                  base::SysNativeMBToWide(config->remote_address_).c_str());
  key->WriteValue(kRemotePortReg, config->remote_port_);
  key->WriteValue(kRemoteSSLReg, config->remote_ssl_);
  key->WriteValue(kRemoteUDPReg, config->remote_udp_);

  return true;
}

std::unique_ptr<ServiceConfig> ScissorsProvider::CopyConfig(
    const ServiceConfig* base_config) {
  if (base_config == nullptr)
    return nullptr;

  return std::make_unique<ScissorsConfig>(
      *static_cast<const ScissorsConfig*>(base_config));
}

std::unique_ptr<base::DictionaryValue> ScissorsProvider::ConvertConfig(
    const ServiceConfig* base_config) {
  if (base_config == nullptr)
    return nullptr;

  auto value = std::make_unique<base::DictionaryValue>();
  if (value == nullptr)
    return nullptr;

  auto config = static_cast<const ScissorsConfig*>(base_config);
  value->SetString(kRemoteAddressJson, config->remote_address_);
  value->SetInteger(kRemotePortJson, config->remote_port_);
  value->SetBoolean(kRemoteSSLJson, config->remote_ssl_);
  value->SetBoolean(kRemoteUDPJson, config->remote_udp_);

  return std::move(value);
}

std::unique_ptr<ServiceConfig> ScissorsProvider::ConvertConfig(
    const base::DictionaryValue* value) {
  if (value == nullptr)
    return nullptr;

  auto config = std::make_unique<ScissorsConfig>();
  if (config == nullptr)
    return nullptr;

  value->GetString(kRemoteAddressJson, &config->remote_address_);
  value->GetInteger(kRemotePortJson, &config->remote_port_);
  value->GetBoolean(kRemoteSSLJson, &config->remote_ssl_);
  value->GetBoolean(kRemoteUDPJson, &config->remote_udp_);

  return std::move(config);
}

std::unique_ptr<Service> ScissorsProvider::CreateService(
    const ServiceConfig* base_config) {
  if (base_config == nullptr)
    return nullptr;

  auto service = std::make_unique<Scissors>();
  if (service == nullptr)
    return nullptr;

  if (!service->UpdateConfig(base_config))
    return nullptr;

  return std::move(service);
}

INT_PTR ScissorsProvider::Configure(ServiceConfig* base_config, HWND parent) {
  return ui::ScissorsDialog(static_cast<ScissorsConfig*>(base_config))
      .DoModal(parent);
}

}  // namespace scissors
}  // namespace service
}  // namespace juno
