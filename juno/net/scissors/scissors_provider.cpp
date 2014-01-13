// Copyright (c) 2014 dacci.org

#include "net/scissors/scissors_provider.h"

#include <memory>

#include "net/scissors/scissors.h"
#include "net/scissors/scissors_config.h"
#include "ui/scissors_dialog.h"

ScissorsProvider::~ScissorsProvider() {
}

ServiceConfig* ScissorsProvider::CreateConfig() {
  return new ScissorsConfig();
}

ServiceConfig* ScissorsProvider::LoadConfig(const RegistryKey& key) {
  std::unique_ptr<ScissorsConfig> config(
      static_cast<ScissorsConfig*>(CreateConfig()));

  if (!config->Load(key))
    return NULL;

  return config.release();
}

bool ScissorsProvider::SaveConfig(ServiceConfig* config, RegistryKey* key) {
  return static_cast<ScissorsConfig*>(config)->Save(key);
}

ServiceConfig* ScissorsProvider::CopyConfig(ServiceConfig* config) {
  return new ScissorsConfig(*static_cast<ScissorsConfig*>(config));
}

bool ScissorsProvider::UpdateConfig(Service* service, ServiceConfig* config) {
  Scissors* scissors = static_cast<Scissors*>(service);
  ScissorsConfig* scissors_config = static_cast<ScissorsConfig*>(config);

  *scissors->config_ = *scissors_config;

  return true;
}

Service* ScissorsProvider::CreateService(ServiceConfig* config) {
  std::unique_ptr<Scissors> service(
    new Scissors(static_cast<ScissorsConfig*>(config)));

  if (!service->Init())
    return NULL;

  return service.release();
}

INT_PTR ScissorsProvider::Configure(ServiceConfig* config, HWND parent) {
  return ScissorsDialog(config).DoModal(parent);
}
