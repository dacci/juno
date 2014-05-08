// Copyright (c) 2014 dacci.org

#ifndef JUNO_NET_HTTP_HTTP_PROXY_PROVIDER_H_
#define JUNO_NET_HTTP_HTTP_PROXY_PROVIDER_H_

#include "app/service_provider.h"

class HttpProxyProvider : public ServiceProvider {
 public:
  virtual ~HttpProxyProvider();

  ServiceConfigPtr CreateConfig() override;
  ServiceConfigPtr LoadConfig(const RegistryKey& key) override;
  bool SaveConfig(const ServiceConfigPtr& config, RegistryKey* key) override;
  ServiceConfigPtr CopyConfig(const ServiceConfigPtr& config) override;

  ServicePtr CreateService(const ServiceConfigPtr& config) override;

  INT_PTR Configure(const ServiceConfigPtr& config, HWND parent) override;
};

#endif  // JUNO_NET_HTTP_HTTP_PROXY_PROVIDER_H_
