// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_HTTP_HTTP_PROXY_PROVIDER_H_
#define JUNO_SERVICE_HTTP_HTTP_PROXY_PROVIDER_H_

#include "service/service_provider.h"

namespace juno {
namespace service {
namespace http {

class HttpProxyProvider : public ServiceProvider {
 public:
  HttpProxyProvider() {}

  std::unique_ptr<ServiceConfig> CreateConfig() override;
  std::unique_ptr<ServiceConfig> LoadConfig(
      const misc::RegistryKey& key) override;
  bool SaveConfig(const ServiceConfig* config, misc::RegistryKey* key) override;
  std::unique_ptr<ServiceConfig> CopyConfig(
      const ServiceConfig* config) override;

  std::unique_ptr<Service> CreateService(const ServiceConfig* config) override;

  INT_PTR Configure(ServiceConfig* config, HWND parent) override;

 private:
  HttpProxyProvider(const HttpProxyProvider&) = delete;
  HttpProxyProvider& operator=(const HttpProxyProvider&) = delete;
};

}  // namespace http
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_HTTP_HTTP_PROXY_PROVIDER_H_
