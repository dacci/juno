// Copyright (c) 2014 dacci.org

#include "service/socks/socks_proxy_provider.h"

#include "service/socks/socks_proxy.h"

ServiceConfigPtr SocksProxyProvider::CopyConfig(
    const ServiceConfigPtr& config) {
  return std::make_shared<SocksProxyConfig>(
      *static_cast<SocksProxyConfig*>(config.get()));
}

ServicePtr SocksProxyProvider::CreateService(
    const ServiceConfigPtr& /*config*/) {
  return std::make_unique<SocksProxy>();
}
