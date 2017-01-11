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

  ServiceConfigPtr CreateConfig() override;
  ServiceConfigPtr LoadConfig(const misc::RegistryKey& key) override;
  bool SaveConfig(const ServiceConfig* config, misc::RegistryKey* key) override;
  ServiceConfigPtr CopyConfig(const ServiceConfig* config) override;

  ServicePtr CreateService(const ServiceConfigPtr& config) override;

  INT_PTR Configure(ServiceConfig* config, HWND parent) override;

 private:
  ScissorsProvider(const ScissorsProvider&) = delete;
  ScissorsProvider& operator=(const ScissorsProvider&) = delete;
};

}  // namespace scissors
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_PROVIDER_H_
