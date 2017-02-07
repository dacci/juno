// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_SERVICE_PROVIDER_H_
#define JUNO_SERVICE_SERVICE_PROVIDER_H_

#include <base/values.h>

#pragma warning(push, 3)
#pragma warning(disable : 4244)
#include <base/win/registry.h>
#pragma warning(pop)

#include <memory>

#ifdef CreateService
#undef CreateService
#endif

namespace juno {
namespace service {

struct ServiceConfig;
class Service;

class __declspec(novtable) ServiceProvider {
 public:
  virtual ~ServiceProvider() {}

  virtual std::unique_ptr<ServiceConfig> CreateConfig() = 0;
  virtual std::unique_ptr<ServiceConfig> LoadConfig(
      const base::win::RegKey& key) = 0;
  virtual bool SaveConfig(const ServiceConfig* base_config,
                          base::win::RegKey* key) = 0;
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
