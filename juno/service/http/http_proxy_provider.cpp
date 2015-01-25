// Copyright (c) 2014 dacci.org

#include "service/http/http_proxy_provider.h"

#include <memory>

#include "service/http/http_proxy.h"
#include "service/http/http_proxy_config.h"
#include "ui/http_proxy_dialog.h"

HttpProxyProvider::~HttpProxyProvider() {
}

ServiceConfigPtr HttpProxyProvider::CreateConfig() {
  return std::make_shared<HttpProxyConfig>();
}

ServiceConfigPtr HttpProxyProvider::LoadConfig(const RegistryKey& key) {
  auto& config = std::static_pointer_cast<HttpProxyConfig>(CreateConfig());
  if (config == nullptr)
    return nullptr;

  if (!config->Load(key))
    return nullptr;

  return config;
}

bool HttpProxyProvider::SaveConfig(const ServiceConfigPtr& config,
                                   RegistryKey* key) {
  return static_cast<HttpProxyConfig*>(config.get())->Save(key);
}

ServiceConfigPtr HttpProxyProvider::CopyConfig(const ServiceConfigPtr& config) {
  return std::make_shared<HttpProxyConfig>(
      *static_cast<HttpProxyConfig*>(config.get()));
}

ServicePtr HttpProxyProvider::CreateService(const ServiceConfigPtr& config) {
  auto service = std::make_unique<HttpProxy>();
  if (service == nullptr)
    return nullptr;

  if (!service->UpdateConfig(config))
    return nullptr;

  return std::move(service);
}

INT_PTR HttpProxyProvider::Configure(const ServiceConfigPtr& config,
                                     HWND parent) {
  return HttpProxyDialog(config).DoModal(parent);
}
