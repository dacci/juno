// Copyright (c) 2014 dacci.org

#include "service/socks/socks_proxy_provider.h"

#include <base/values.h>

#include "service/socks/socks_proxy.h"

namespace juno {
namespace service {
namespace socks {

std::unique_ptr<ServiceConfig> SocksProxyProvider::CreateConfig() {
  return std::make_unique<SocksProxyConfig>();
}

std::unique_ptr<ServiceConfig> SocksProxyProvider::LoadConfig(
    const base::win::RegKey& /*key*/) {
  return CreateConfig();
}

bool SocksProxyProvider::SaveConfig(const ServiceConfig* base_config,
                                    base::win::RegKey* key) {
  if (base_config == nullptr || key == nullptr)
    return false;

  // Nothing to save.

  return true;
}

std::unique_ptr<ServiceConfig> SocksProxyProvider::CopyConfig(
    const ServiceConfig* base_config) {
  if (base_config == nullptr)
    return nullptr;

  return std::make_unique<SocksProxyConfig>(
      *static_cast<const SocksProxyConfig*>(base_config));
}

std::unique_ptr<base::DictionaryValue> SocksProxyProvider::ConvertConfig(
    const ServiceConfig* base_config) {
  if (base_config == nullptr)
    return nullptr;

  return std::make_unique<base::DictionaryValue>();
}

std::unique_ptr<ServiceConfig> SocksProxyProvider::ConvertConfig(
    const base::DictionaryValue* value) {
  if (value == nullptr)
    return nullptr;

  return std::make_unique<SocksProxyConfig>();
}

std::unique_ptr<Service> SocksProxyProvider::CreateService(
    const ServiceConfig* base_config) {
  if (base_config == nullptr)
    return nullptr;

  return std::make_unique<SocksProxy>();
}

INT_PTR SocksProxyProvider::Configure(ServiceConfig* /*base_config*/,
                                      HWND /*parent*/) {
  return IDOK;
}

}  // namespace socks
}  // namespace service
}  // namespace juno
