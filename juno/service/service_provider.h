// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_SERVICE_PROVIDER_H_
#define JUNO_SERVICE_SERVICE_PROVIDER_H_

#include <memory>

#include "misc/registry_key.h"
#include "service/service.h"
#include "service/service_config.h"

#ifdef CreateService
#undef CreateService
#endif

namespace juno {
namespace service {

class __declspec(novtable) ServiceProvider {
 public:
  virtual ~ServiceProvider() {}

  virtual ServiceConfigPtr CreateConfig() = 0;
  virtual ServiceConfigPtr LoadConfig(const misc::RegistryKey& key) = 0;
  virtual bool SaveConfig(const ServiceConfigPtr& config,
                          misc::RegistryKey* key) = 0;
  virtual ServiceConfigPtr CopyConfig(const ServiceConfigPtr& config) = 0;

  virtual ServicePtr CreateService(const ServiceConfigPtr& config) = 0;

  virtual INT_PTR Configure(const ServiceConfigPtr& config, HWND parent) = 0;
};

typedef std::shared_ptr<ServiceProvider> ServiceProviderPtr;

}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SERVICE_PROVIDER_H_
