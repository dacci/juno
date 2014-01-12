// Copyright (c) 2014 dacci.org

#ifndef JUNO_NET_SOCKS_SOCKS_PROXY_PROVIDER_H_
#define JUNO_NET_SOCKS_SOCKS_PROXY_PROVIDER_H_

#include "app/service_provider.h"
#include "app/service_config.h"

class SocksProxyConfig : public ServiceConfig {
};

class SocksProxyProvider : public ServiceProvider {
 public:
  virtual ~SocksProxyProvider();

  ServiceConfig* LoadConfig(const RegistryKey& key);
  bool LoadConfig(ServiceConfig* config, const RegistryKey& key);
  ServiceConfig* CopyConfig(ServiceConfig* config);

  Service* CreateService(ServiceConfig* config);

  INT_PTR Configure(ServiceConfig* config, HWND parent);
};

#endif  // JUNO_NET_SOCKS_SOCKS_PROXY_PROVIDER_H_
