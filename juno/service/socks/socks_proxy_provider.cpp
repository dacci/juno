// Copyright (c) 2014 dacci.org

#include "service/socks/socks_proxy_provider.h"

#include "service/socks/socks_proxy.h"

SocksProxyProvider::~SocksProxyProvider() {
}

ServiceConfigPtr SocksProxyProvider::CreateConfig() {
  return std::make_shared<SocksProxyConfig>();
}

ServiceConfigPtr SocksProxyProvider::LoadConfig(const RegistryKey& key) {
  return CreateConfig();
}

bool SocksProxyProvider::SaveConfig(const ServiceConfigPtr& config,
                                    RegistryKey* key) {
  return true;
}

ServiceConfigPtr SocksProxyProvider::CopyConfig(
    const ServiceConfigPtr& config) {
  return std::make_shared<SocksProxyConfig>(
      *static_cast<SocksProxyConfig*>(config.get()));
}

ServicePtr SocksProxyProvider::CreateService(const ServiceConfigPtr& config) {
  return ServicePtr(new SocksProxy());
}

INT_PTR SocksProxyProvider::Configure(const ServiceConfigPtr& config,
                                      HWND parent) {
  return IDOK;
}
