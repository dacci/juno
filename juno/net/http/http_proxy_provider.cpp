// Copyright (c) 2014 dacci.org

#include "net/http/http_proxy_provider.h"

#include <memory>

#include "net/http/http_proxy.h"
#include "net/http/http_proxy_config.h"

HttpProxyProvider::~HttpProxyProvider() {
}

ServiceConfig* HttpProxyProvider::LoadConfig(const RegistryKey& key) {
  std::unique_ptr<HttpProxyConfig> config(new HttpProxyConfig());

  if (!config->Load(key))
    return NULL;

  return config.release();
}

bool HttpProxyProvider::LoadConfig(ServiceConfig* config,
                                   const RegistryKey& key) {
  return false;
}

ServiceConfig* HttpProxyProvider::CopyConfig(ServiceConfig* config) {
  return new HttpProxyConfig(*static_cast<HttpProxyConfig*>(config));
}

Service* HttpProxyProvider::CreateService(ServiceConfig* config) {
  std::unique_ptr<HttpProxy> service(
    new HttpProxy(static_cast<HttpProxyConfig*>(config)));

  if (!service->Init())
    return NULL;

  return service.release();
}

INT_PTR HttpProxyProvider::Configure(ServiceConfig* config, HWND parent) {
  return IDCANCEL;
}
