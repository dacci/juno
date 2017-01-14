// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_PROVIDER_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_PROVIDER_H_

#include "service/service_provider.h"

namespace juno {
namespace service {
namespace scissors {

class ScissorsProvider : public ServiceProvider {
 public:
  ScissorsProvider() {}

  std::unique_ptr<ServiceConfig> CreateConfig() override;
  std::unique_ptr<ServiceConfig> LoadConfig(
      const base::win::RegKey& key) override;
  bool SaveConfig(const ServiceConfig* base_config,
                  base::win::RegKey* key) override;
  std::unique_ptr<ServiceConfig> CopyConfig(
      const ServiceConfig* base_config) override;

  std::unique_ptr<base::DictionaryValue> ConvertConfig(
      const ServiceConfig* base_config) override;
  std::unique_ptr<ServiceConfig> ConvertConfig(
      const base::DictionaryValue* value) override;

  std::unique_ptr<Service> CreateService(
      const ServiceConfig* base_config) override;

  INT_PTR Configure(ServiceConfig* base_config, HWND parent) override;

 private:
  ScissorsProvider(const ScissorsProvider&) = delete;
  ScissorsProvider& operator=(const ScissorsProvider&) = delete;
};

}  // namespace scissors
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_PROVIDER_H_
