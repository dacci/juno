// Copyright (c) 2014 dacci.org

#include "service/scissors/scissors_config.h"

#include <string>

namespace juno {
namespace service {
namespace scissors {
namespace {

const char kRemoteAddress[] = "RemoteAddress";
const char kRemotePort[] = "RemotePort";
const char kRemoteSSL[] = "RemoteSSL";
const char kRemoteUDP[] = "RemoteUDP";

}  // namespace

ScissorsConfig::ScissorsConfig()
    : remote_port_(), remote_ssl_(), remote_udp_() {}

ScissorsConfig::ScissorsConfig(const ScissorsConfig& other)
    : ServiceConfig(other),
      remote_address_(other.remote_address_),
      remote_port_(other.remote_port_),
      remote_ssl_(other.remote_ssl_),
      remote_udp_(other.remote_udp_) {}

std::unique_ptr<ScissorsConfig> ScissorsConfig::Load(
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

bool ScissorsConfig::Save(misc::RegistryKey* key) const {
  key->SetString(kRemoteAddress, remote_address_);
  key->SetInteger(kRemotePort, remote_port_);
  key->SetInteger(kRemoteSSL, remote_ssl_);
  key->SetInteger(kRemoteUDP, remote_udp_);

  return true;
}

}  // namespace scissors
}  // namespace service
}  // namespace juno
