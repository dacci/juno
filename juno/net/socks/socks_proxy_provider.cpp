// Copyright (c) 2014 dacci.org

#include "net/socks/socks_proxy_provider.h"

#include "net/socks/socks_proxy.h"

SocksProxyProvider::~SocksProxyProvider() {
}

ServiceConfig* SocksProxyProvider::LoadConfig(const RegistryKey& key) {
  return new SocksProxyConfig();
}

bool SocksProxyProvider::LoadConfig(ServiceConfig* config,
                                    const RegistryKey& key) {
  return true;
}

ServiceConfig* SocksProxyProvider::CopyConfig(ServiceConfig* config) {
  return new SocksProxyConfig(*static_cast<SocksProxyConfig*>(config));
}

Service* SocksProxyProvider::CreateService(ServiceConfig* config) {
  return new SocksProxy(static_cast<SocksProxyConfig*>(config));
}

INT_PTR SocksProxyProvider::Configure(ServiceConfig* config, HWND parent) {
  return IDCANCEL;
}
