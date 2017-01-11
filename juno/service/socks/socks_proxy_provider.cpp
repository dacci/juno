// Copyright (c) 2014 dacci.org

#include "service/socks/socks_proxy_provider.h"

#include "service/socks/socks_proxy.h"

namespace juno {
namespace service {
namespace socks {

ServiceConfigPtr SocksProxyProvider::CopyConfig(const ServiceConfig* config) {
  return std::make_shared<SocksProxyConfig>(
      *static_cast<const SocksProxyConfig*>(config));
}

ServicePtr SocksProxyProvider::CreateService(
    const ServiceConfigPtr& /*config*/) {
  return std::make_unique<SocksProxy>();
}

}  // namespace socks
}  // namespace service
}  // namespace juno
