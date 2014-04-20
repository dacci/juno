// Copyright (c) 2014 dacci.org

#ifndef JUNO_NET_HTTP_HTTP_PROXY_PROVIDER_H_
#define JUNO_NET_HTTP_HTTP_PROXY_PROVIDER_H_

#include "app/service_provider.h"

class HttpProxyProvider : public ServiceProvider {
 public:
  virtual ~HttpProxyProvider();

  ServiceConfig* CreateConfig() override;
  ServiceConfig* LoadConfig(const RegistryKey& key) override;
  bool SaveConfig(ServiceConfig* config, RegistryKey* key) override;
  ServiceConfig* CopyConfig(ServiceConfig* config) override;
  bool UpdateConfig(Service* service, ServiceConfig* config) override;

  Service* CreateService(ServiceConfig* config) override;

  INT_PTR Configure(ServiceConfig* config, HWND parent) override;
};

#endif  // JUNO_NET_HTTP_HTTP_PROXY_PROVIDER_H_
