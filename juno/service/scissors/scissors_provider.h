// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_PROVIDER_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_PROVIDER_H_

#include "service/service_provider.h"

class ScissorsProvider : public ServiceProvider {
 public:
  ScissorsProvider() {}

  ServiceConfigPtr CreateConfig() override;
  ServiceConfigPtr LoadConfig(const RegistryKey& key) override;
  bool SaveConfig(const ServiceConfigPtr& config, RegistryKey* key) override;
  ServiceConfigPtr CopyConfig(const ServiceConfigPtr& config) override;

  ServicePtr CreateService(const ServiceConfigPtr& config) override;

  INT_PTR Configure(const ServiceConfigPtr& config, HWND parent) override;

 private:
  ScissorsProvider(const ScissorsProvider&) = delete;
  ScissorsProvider& operator=(const ScissorsProvider&) = delete;
};

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_PROVIDER_H_
