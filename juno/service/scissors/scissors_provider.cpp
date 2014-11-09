// Copyright (c) 2014 dacci.org

#include "service/scissors/scissors_provider.h"

#include <memory>

#include "service/scissors/scissors.h"
#include "service/scissors/scissors_config.h"
#include "ui/scissors_dialog.h"

ScissorsProvider::~ScissorsProvider() {
}

ServiceConfigPtr ScissorsProvider::CreateConfig() {
  return std::make_shared<ScissorsConfig>();
}

ServiceConfigPtr ScissorsProvider::LoadConfig(const RegistryKey& key) {
  auto& config = std::static_pointer_cast<ScissorsConfig>(CreateConfig());
  if (config == nullptr)
    return nullptr;

  if (!config->Load(key))
    return nullptr;

  return config;
}

bool ScissorsProvider::SaveConfig(const ServiceConfigPtr& config,
                                  RegistryKey* key) {
  return static_cast<ScissorsConfig*>(config.get())->Save(key);
}

ServiceConfigPtr ScissorsProvider::CopyConfig(const ServiceConfigPtr& config) {
  return std::make_shared<ScissorsConfig>(
      *static_cast<ScissorsConfig*>(config.get()));
}

ServicePtr ScissorsProvider::CreateService(const ServiceConfigPtr& config) {
  std::unique_ptr<Scissors> service(new Scissors(config));
  if (service == nullptr)
    return nullptr;

  if (!service->Init())
    return nullptr;

  return std::move(service);
}

INT_PTR ScissorsProvider::Configure(const ServiceConfigPtr& config,
                                    HWND parent) {
  return ScissorsDialog(config.get()).DoModal(parent);
}
