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

namespace base {

class DictionaryValue;

}  // namespace base

namespace juno {
namespace service {

class __declspec(novtable) ServiceProvider {
 public:
  virtual ~ServiceProvider() {}

  virtual std::unique_ptr<ServiceConfig> CreateConfig() = 0;
  virtual std::unique_ptr<ServiceConfig> LoadConfig(
      const misc::RegistryKey& key) = 0;
  virtual bool SaveConfig(const ServiceConfig* base_config,
                          misc::RegistryKey* key) = 0;
  virtual std::unique_ptr<ServiceConfig> CopyConfig(
      const ServiceConfig* base_config) = 0;

  virtual std::unique_ptr<base::DictionaryValue> ConvertConfig(
      const ServiceConfig* base_config) = 0;
  virtual std::unique_ptr<ServiceConfig> ConvertConfig(
      const base::DictionaryValue* value) = 0;

  virtual std::unique_ptr<Service> CreateService(
      const ServiceConfig* base_config) = 0;

  virtual INT_PTR Configure(ServiceConfig* base_config, HWND parent) = 0;
};

}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SERVICE_PROVIDER_H_
