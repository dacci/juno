// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_SOCKS_SOCKS_PROXY_PROVIDER_H_
#define JUNO_SERVICE_SOCKS_SOCKS_PROXY_PROVIDER_H_

#include "service/service_config.h"
#include "service/service_provider.h"

class SocksProxyConfig : public ServiceConfig {
};

class SocksProxyProvider : public ServiceProvider {
 public:
  virtual ~SocksProxyProvider();

  ServiceConfigPtr CreateConfig() override;
  ServiceConfigPtr LoadConfig(const RegistryKey& key) override;
  bool SaveConfig(const ServiceConfigPtr& config, RegistryKey* key) override;
  ServiceConfigPtr CopyConfig(const ServiceConfigPtr& config) override;

  ServicePtr CreateService(const ServiceConfigPtr& config) override;

  INT_PTR Configure(const ServiceConfigPtr& config, HWND parent) override;
};

#endif  // JUNO_SERVICE_SOCKS_SOCKS_PROXY_PROVIDER_H_
