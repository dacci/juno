// Copyright (c) 2014 dacci.org

#ifndef JUNO_APP_SERVICE_PROVIDER_H_
#define JUNO_APP_SERVICE_PROVIDER_H_

#include "misc/registry_key.h"

#ifdef CreateService
#undef CreateService
#endif

class Service;
class ServiceConfig;

class ServiceProvider {
 public:
  virtual ~ServiceProvider() {}

  virtual ServiceConfig* LoadConfig(const RegistryKey& key) = 0;
  virtual bool LoadConfig(ServiceConfig* config, const RegistryKey& key) = 0;
  virtual ServiceConfig* CopyConfig(ServiceConfig* config) = 0;

  virtual Service* CreateService(ServiceConfig* config) = 0;

  virtual INT_PTR Configure(ServiceConfig* config, HWND parent) = 0;
};

#endif  // JUNO_APP_SERVICE_PROVIDER_H_
