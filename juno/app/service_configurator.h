// Copyright (c) 2017 dacci.org

#ifndef JUNO_APP_SERVICE_CONFIGURATOR_H_
#define JUNO_APP_SERVICE_CONFIGURATOR_H_

#include <memory>

#include "service/service_manager.h"
#include "service/rpc/rpc_client.h"

namespace juno {
namespace app {

class ServiceConfigurator : private service::rpc::RpcClient::Listener {
 public:
  ServiceConfigurator();
  ~ServiceConfigurator();

  void Configure();

 private:
  static const DWORD kTimeout = 3000;

  void OnReturned(const char* method, const base::Value* result,
                  const base::DictionaryValue* error) override;

  HANDLE event_;
  std::unique_ptr<service::ServiceManager> manager_;
  bool loaded_;
  bool saved_;

  ServiceConfigurator(const ServiceConfigurator&) = delete;
  ServiceConfigurator& operator=(const ServiceConfigurator&) = delete;
};

}  // namespace app
}  // namespace juno

#endif  // JUNO_APP_SERVICE_CONFIGURATOR_H_
