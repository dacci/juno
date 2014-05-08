// Copyright (c) 2014 dacci.org

#ifndef JUNO_APP_SERVICE_PROVIDER_H_
#define JUNO_APP_SERVICE_PROVIDER_H_

#include <memory>

#include "app/service.h"
#include "app/service_config.h"
#include "misc/registry_key.h"

#ifdef CreateService
#undef CreateService
#endif

class ServiceProvider {
 public:
  virtual ~ServiceProvider() {}

  virtual ServiceConfigPtr CreateConfig() = 0;
  virtual ServiceConfigPtr LoadConfig(const RegistryKey& key) = 0;
  virtual bool SaveConfig(const ServiceConfigPtr& config, RegistryKey* key) = 0;
  virtual ServiceConfigPtr CopyConfig(const ServiceConfigPtr& config) = 0;

  virtual ServicePtr CreateService(const ServiceConfigPtr& config) = 0;

  virtual INT_PTR Configure(const ServiceConfigPtr& config, HWND parent) = 0;
};

typedef std::shared_ptr<ServiceProvider> ServiceProviderPtr;

#endif  // JUNO_APP_SERVICE_PROVIDER_H_
