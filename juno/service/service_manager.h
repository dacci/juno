// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SERVICE_MANAGER_H_
#define JUNO_SERVICE_SERVICE_MANAGER_H_

#include <base/values.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "misc/registry_key.h"
#include "service/server_config.h"
#include "service/service_provider.h"

namespace juno {
namespace service {

class Server;

typedef std::map<std::string, std::unique_ptr<ServiceProvider>> ProviderMap;
typedef std::map<std::string, std::unique_ptr<ServiceConfig>> ServiceConfigMap;
typedef std::map<std::string, std::unique_ptr<ServerConfig>> ServerConfigMap;

class ServiceManager {
 public:
  ServiceManager();
  virtual ~ServiceManager();

  static ServiceManager* GetInstance() {
    return instance_;
  }

  bool LoadServices();
  void StopServices();

  bool LoadServers();
  bool StartServers();
  void StopServers();

  ServiceProvider* GetProvider(const std::string& name) const;
  void CopyServiceConfigs(ServiceConfigMap* configs) const;
  void CopyServerConfigs(ServerConfigMap* configs) const;

  bool UpdateConfiguration(
      ServiceConfigMap&& service_configs,  // NOLINT(whitespace/operators)
      ServerConfigMap&& server_configs);   // NOLINT(whitespace/operators)

  std::unique_ptr<base::ListValue> ConvertServiceConfig(
      const ServiceConfigMap* configs) const;
  std::unique_ptr<base::DictionaryValue> ConvertServiceConfig(
      const ServiceConfig* config) const;
  bool ConvertServiceConfig(const base::ListValue* values,
                            ServiceConfigMap* output) const;
  std::unique_ptr<ServiceConfig> ConvertServiceConfig(
      const base::DictionaryValue* value) const;

  static std::unique_ptr<base::ListValue> ConvertServerConfig(
      const ServerConfigMap* configs);
  static std::unique_ptr<base::DictionaryValue> ConvertServerConfig(
      const ServerConfig* config);
  static bool ConvertServerConfig(const base::ListValue* values,
                                  ServerConfigMap* output);
  static std::unique_ptr<ServerConfig> ConvertServerConfig(
      const base::DictionaryValue* value);

  const ProviderMap& providers() const {
    return providers_;
  }

 private:
  typedef std::map<std::string, std::unique_ptr<Service>> ServiceMap;
  typedef std::map<std::string, std::unique_ptr<Server>> ServerMap;

  bool LoadService(const misc::RegistryKey& parent, const std::string& id);
  bool SaveService(const misc::RegistryKey& parent,
                   const ServiceConfig* config);
  bool CreateService(const std::string& id);

  bool LoadServer(const misc::RegistryKey& parent, const std::string& id);
  bool CreateServer(const std::string& id);
  static bool SaveServer(const misc::RegistryKey& parent,
                         const ServerConfig* config);

  static void GetConfig(void* context, const base::Value* params,
                        base::DictionaryValue* response);
  static void SetConfig(void* context, const base::Value* params,
                        base::DictionaryValue* response);

  static ServiceManager* instance_;

  HKEY root_key_;

  ProviderMap providers_;
  ServiceConfigMap service_configs_;
  ServerConfigMap server_configs_;
  ServiceMap services_;
  ServerMap servers_;

  ServiceManager(const ServiceManager&) = delete;
  ServiceManager& operator=(const ServiceManager&) = delete;
};

}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SERVICE_MANAGER_H_
