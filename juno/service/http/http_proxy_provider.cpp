// Copyright (c) 2014 dacci.org

#include "service/http/http_proxy_provider.h"

#include <memory>

#include "service/http/http_proxy.h"
#include "service/http/http_proxy_config.h"
#include "ui/http_proxy_dialog.h"

namespace juno {
namespace service {
namespace http {

ServiceConfigPtr HttpProxyProvider::CreateConfig() {
  return std::make_shared<HttpProxyConfig>();
}

ServiceConfigPtr HttpProxyProvider::LoadConfig(const misc::RegistryKey& key) {
  return HttpProxyConfig::Load(key);
}

bool HttpProxyProvider::SaveConfig(const ServiceConfig* config,
                                   misc::RegistryKey* key) {
  return static_cast<const HttpProxyConfig*>(config)->Save(key);
}

ServiceConfigPtr HttpProxyProvider::CopyConfig(const ServiceConfig* config) {
  return std::make_shared<HttpProxyConfig>(
      *static_cast<const HttpProxyConfig*>(config));
}

ServicePtr HttpProxyProvider::CreateService(const ServiceConfigPtr& config) {
  auto service = std::make_unique<HttpProxy>();
  if (service == nullptr)
    return nullptr;

  if (!service->UpdateConfig(config))
    return nullptr;

  return std::move(service);
}

INT_PTR HttpProxyProvider::Configure(ServiceConfig* config, HWND parent) {
  return ui::HttpProxyDialog(static_cast<HttpProxyConfig*>(config))
      .DoModal(parent);
}

}  // namespace http
}  // namespace service
}  // namespace juno
