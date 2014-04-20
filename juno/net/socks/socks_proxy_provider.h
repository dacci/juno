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

  ServiceConfig* CreateConfig() override;
  ServiceConfig* LoadConfig(const RegistryKey& key) override;
  bool SaveConfig(ServiceConfig* config, RegistryKey* key) override;
  ServiceConfig* CopyConfig(ServiceConfig* config) override;
  bool UpdateConfig(Service* service, ServiceConfig* config) override;

  Service* CreateService(ServiceConfig* config) override;

  INT_PTR Configure(ServiceConfig* config, HWND parent) override;
};

#endif  // JUNO_NET_SOCKS_SOCKS_PROXY_PROVIDER_H_
