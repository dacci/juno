// Copyright (c) 2014 dacci.org

#include "net/http/http_proxy_provider.h"

#include <memory>

#include "net/http/http_proxy.h"
#include "net/http/http_proxy_config.h"
#include "ui/http_proxy_dialog.h"

HttpProxyProvider::~HttpProxyProvider() {
}

ServiceConfig* HttpProxyProvider::CreateConfig() {
  return new HttpProxyConfig();
}

ServiceConfig* HttpProxyProvider::LoadConfig(const RegistryKey& key) {
  std::unique_ptr<HttpProxyConfig> config(
      static_cast<HttpProxyConfig*>(CreateConfig()));

  if (!config->Load(key))
    return NULL;

  return config.release();
}

bool HttpProxyProvider::SaveConfig(ServiceConfig* config, RegistryKey* key) {
  return static_cast<HttpProxyConfig*>(config)->Save(key);
}

ServiceConfig* HttpProxyProvider::CopyConfig(ServiceConfig* config) {
  return new HttpProxyConfig(*static_cast<HttpProxyConfig*>(config));
}

bool HttpProxyProvider::UpdateConfig(Service* service, ServiceConfig* config) {
  HttpProxy* instance = static_cast<HttpProxy*>(service);
  HttpProxyConfig* proxy_config = static_cast<HttpProxyConfig*>(config);

  *instance->config_ = *proxy_config;

  return true;
}

  Service* HttpProxyProvider::CreateService(ServiceConfig* config) {
  std::unique_ptr<HttpProxy> service(
    new HttpProxy(static_cast<HttpProxyConfig*>(config)));

  if (!service->Init())
    return NULL;

  return service.release();
}

INT_PTR HttpProxyProvider::Configure(ServiceConfig* config, HWND parent) {
  return HttpProxyDialog(config).DoModal(parent);
}
