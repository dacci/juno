// Copyright (c) 2013 dacci.org

#ifndef JUNO_APP_SERVICE_MANAGER_H_
#define JUNO_APP_SERVICE_MANAGER_H_

#include <windows.h>

#include <map>
#include <string>
#include <vector>

#include "misc/registry_key.h"

#ifdef CreateService
#undef CreateService
#endif

class ServiceProvider;
class ServiceConfig;
class Service;
class ServerConfig;
class Server;

class ServiceManager {
 public:
  ServiceManager();
  virtual ~ServiceManager();

  bool LoadServices();
  void UnloadServices();
  void StopServices();

  bool LoadServers();
  bool StartServers();
  void StopServers();
  void UnloadServers();
  void ClearServers();

  ServiceProvider* GetProvider(const std::string& name) const;

  ServiceConfig* GetServiceConfig(const std::string& name) const;
  void CopyServiceConfigs(std::map<std::string, ServiceConfig*>* configs) const;

  void CopyServerConfigs(std::map<std::string, ServerConfig*>* configs) const;

  bool UpdateConfiguration(
      std::map<std::string, ServiceConfig*>&& service_configs,
      std::map<std::string, ServerConfig*>&& server_configs);

  const std::map<std::string, ServiceProvider*>& providers() const {
    return providers_;
  }

 private:
  bool LoadService(const RegistryKey& parent, const std::string& key_name);
  bool SaveService(const RegistryKey& parent, ServiceConfig* config);
  bool CreateService(ServiceConfig* config);

  bool LoadServer(const RegistryKey& parent, const std::string& key_name);
  bool SaveServer(const RegistryKey& parent, ServerConfig* config);
  bool CreateServer(ServerConfig* config);

  std::map<std::string, ServiceProvider*> providers_;
  std::map<std::string, ServiceConfig*> services_;
  std::map<std::string, ServerConfig*> servers_;
};

extern ServiceManager* service_manager;

#endif  // JUNO_APP_SERVICE_MANAGER_H_
