// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_SOCKS_SOCKS_PROXY_PROVIDER_H_
#define JUNO_SERVICE_SOCKS_SOCKS_PROXY_PROVIDER_H_

#include "service/service_config.h"
#include "service/service_provider.h"

namespace juno {
namespace service {
namespace socks {

class SocksProxyConfig : public ServiceConfig {};

class SocksProxyProvider : public ServiceProvider {
 public:
  SocksProxyProvider() {}

  ServiceConfigPtr CreateConfig() override {
    return std::make_shared<SocksProxyConfig>();
  }

  ServiceConfigPtr LoadConfig(const misc::RegistryKey& /*key*/) override {
    return CreateConfig();
  }

  bool SaveConfig(const ServiceConfigPtr& /*config*/,
                  misc::RegistryKey* /*key*/) override {
    return true;
  }

  ServiceConfigPtr CopyConfig(const ServiceConfigPtr& config) override;

  ServicePtr CreateService(const ServiceConfigPtr& config) override;

  INT_PTR Configure(const ServiceConfigPtr& /*config*/,
                    HWND /*parent*/) override {
    return IDOK;
  }

 private:
  SocksProxyProvider(const SocksProxyProvider&) = delete;
  SocksProxyProvider& operator=(const SocksProxyProvider&) = delete;
};

}  // namespace socks
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SOCKS_SOCKS_PROXY_PROVIDER_H_
