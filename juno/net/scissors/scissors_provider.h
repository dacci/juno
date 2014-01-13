// Copyright (c) 2014 dacci.org

#ifndef JUNO_NET_SCISSORS_SCISSORS_PROVIDER_H_
#define JUNO_NET_SCISSORS_SCISSORS_PROVIDER_H_

#include "app/service_provider.h"

class ScissorsProvider : public ServiceProvider {
 public:
  virtual ~ScissorsProvider();

  ServiceConfig* CreateConfig();
  ServiceConfig* LoadConfig(const RegistryKey& key);
  bool SaveConfig(ServiceConfig* config, RegistryKey* key);
  ServiceConfig* CopyConfig(ServiceConfig* config);
  bool UpdateConfig(Service* service, ServiceConfig* config);

  Service* CreateService(ServiceConfig* config);

  INT_PTR Configure(ServiceConfig* config, HWND parent);
};

#endif  // JUNO_NET_SCISSORS_SCISSORS_PROVIDER_H_
