// Copyright (c) 2014 dacci.org

#include "service/socks/socks_proxy_provider.h"

#include "service/socks/socks_proxy.h"

namespace juno {
namespace service {
namespace socks {

std::unique_ptr<ServiceConfig> SocksProxyProvider::CopyConfig(
    const ServiceConfig* config) {
  return std::make_unique<SocksProxyConfig>(
      *static_cast<const SocksProxyConfig*>(config));
}

std::unique_ptr<Service> SocksProxyProvider::CreateService(
    const ServiceConfig* /*config*/) {
  return std::make_unique<SocksProxy>();
}

}  // namespace socks
}  // namespace service
}  // namespace juno
