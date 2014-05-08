// Copyright (c) 2014 dacci.org

#include "net/socks/socks_proxy_provider.h"

#include "net/socks/socks_proxy.h"

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
  // nothing to copy for now
  return CreateConfig();
}

ServicePtr SocksProxyProvider::CreateService(const ServiceConfigPtr& config) {
  return ServicePtr(new SocksProxy());
}

INT_PTR SocksProxyProvider::Configure(const ServiceConfigPtr& config,
                                      HWND parent) {
  return IDOK;
}
