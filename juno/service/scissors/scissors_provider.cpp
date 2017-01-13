// Copyright (c) 2014 dacci.org

#include "service/scissors/scissors_provider.h"

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

const char kRemoteAddress[] = "RemoteAddress";
const char kRemotePort[] = "RemotePort";
const char kRemoteSSL[] = "RemoteSSL";
const char kRemoteUDP[] = "RemoteUDP";

const std::string kRemoteAddressJson = "remote_address";
const std::string kRemotePortJson = "remote_port";
const std::string kRemoteSSLJson = "remote_ssl";
const std::string kRemoteUDPJson = "remote_udp";

}  // namespace

std::unique_ptr<ServiceConfig> ScissorsProvider::CreateConfig() {
  return std::make_unique<ScissorsConfig>();
}

std::unique_ptr<ServiceConfig> ScissorsProvider::LoadConfig(
    const misc::RegistryKey& key) {
  auto config = std::make_unique<ScissorsConfig>();
  if (config == nullptr)
    return nullptr;

  int int_value;
  std::string string_value;

  if (!key.QueryString(kRemoteAddress, &string_value) ||
      !key.QueryInteger(kRemotePort, &int_value))
    return nullptr;

  config->remote_address_ = string_value;
  config->remote_port_ = int_value;

  if (key.QueryInteger(kRemoteSSL, &int_value))
    config->remote_ssl_ = int_value != 0;

  if (key.QueryInteger(kRemoteUDP, &int_value))
    config->remote_udp_ = int_value != 0;

  return std::move(config);
}

bool ScissorsProvider::SaveConfig(const ServiceConfig* base_config,
                                  misc::RegistryKey* key) {
  if (base_config == nullptr || key == nullptr)
    return false;

  auto config = static_cast<const ScissorsConfig*>(base_config);

  key->SetString(kRemoteAddress, config->remote_address_);
  key->SetInteger(kRemotePort, config->remote_port_);
  key->SetInteger(kRemoteSSL, config->remote_ssl_);
  key->SetInteger(kRemoteUDP, config->remote_udp_);

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

  value->GetString(kRemoteAddress, &config->remote_address_);
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
