// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_HTTP_HTTP_PROXY_PROVIDER_H_
#define JUNO_SERVICE_HTTP_HTTP_PROXY_PROVIDER_H_

#include "service/service_provider.h"

class HttpProxyProvider : public ServiceProvider {
 public:
  HttpProxyProvider() {}

  ServiceConfigPtr CreateConfig() override;
  ServiceConfigPtr LoadConfig(const RegistryKey& key) override;
  bool SaveConfig(const ServiceConfigPtr& config, RegistryKey* key) override;
  ServiceConfigPtr CopyConfig(const ServiceConfigPtr& config) override;

  ServicePtr CreateService(const ServiceConfigPtr& config) override;

  INT_PTR Configure(const ServiceConfigPtr& config, HWND parent) override;

 private:
  HttpProxyProvider(const HttpProxyProvider&) = delete;
  HttpProxyProvider& operator=(const HttpProxyProvider&) = delete;
};

#endif  // JUNO_SERVICE_HTTP_HTTP_PROXY_PROVIDER_H_
