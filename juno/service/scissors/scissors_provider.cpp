// Copyright (c) 2014 dacci.org

#include "service/scissors/scissors_provider.h"

#include <memory>

#include "service/scissors/scissors.h"
#include "service/scissors/scissors_config.h"
#include "ui/scissors_dialog.h"

namespace juno {
namespace service {
namespace scissors {

ServiceConfigPtr ScissorsProvider::CreateConfig() {
  return std::make_shared<ScissorsConfig>();
}

ServiceConfigPtr ScissorsProvider::LoadConfig(const misc::RegistryKey& key) {
  return ScissorsConfig::Load(key);
}

bool ScissorsProvider::SaveConfig(const ServiceConfig* config,
                                  misc::RegistryKey* key) {
  return static_cast<const ScissorsConfig*>(config)->Save(key);
}

ServiceConfigPtr ScissorsProvider::CopyConfig(const ServiceConfig* config) {
  return std::make_shared<ScissorsConfig>(
      *static_cast<const ScissorsConfig*>(config));
}

ServicePtr ScissorsProvider::CreateService(const ServiceConfigPtr& config) {
  auto service = std::make_unique<Scissors>();
  if (service == nullptr)
    return nullptr;

  if (!service->UpdateConfig(config))
    return nullptr;

  return std::move(service);
}

INT_PTR ScissorsProvider::Configure(ServiceConfig* config, HWND parent) {
  return ui::ScissorsDialog(static_cast<ScissorsConfig*>(config))
      .DoModal(parent);
}

}  // namespace scissors
}  // namespace service
}  // namespace juno
