// Copyright (c) 2014 dacci.org

#include "net/scissors/scissors_provider.h"

#include <memory>

#include "net/scissors/scissors.h"
#include "net/scissors/scissors_config.h"

ScissorsProvider::~ScissorsProvider() {
}

ServiceConfig* ScissorsProvider::LoadConfig(const RegistryKey& key) {
  std::unique_ptr<ScissorsConfig> config(new ScissorsConfig());

  if (!config->Load(key))
    return NULL;

  return config.release();
}

bool ScissorsProvider::LoadConfig(ServiceConfig* config,
                                  const RegistryKey& key) {
  return false;
}

ServiceConfig* ScissorsProvider::CopyConfig(ServiceConfig* config) {
  return new ScissorsConfig(*static_cast<ScissorsConfig*>(config));
}

Service* ScissorsProvider::CreateService(ServiceConfig* config) {
  std::unique_ptr<Scissors> service(
    new Scissors(static_cast<ScissorsConfig*>(config)));

  if (!service->Init())
    return NULL;

  return service.release();
}

INT_PTR ScissorsProvider::Configure(ServiceConfig* config, HWND parent) {
  return IDCANCEL;
}
