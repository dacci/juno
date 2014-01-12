// Copyright (c) 2013 dacci.org

#ifndef JUNO_APP_SERVICE_MANAGER_H_
#define JUNO_APP_SERVICE_MANAGER_H_

#include <windows.h>

#include <map>
#include <string>
#include <vector>

class ServiceProvider;
class ServiceConfig;
class Service;
class Server;

class ServiceManager {
 public:
  ServiceManager();
  virtual ~ServiceManager();

  bool LoadServices();
  void UnloadServices();
  void StopServices();

  bool LoadServers();
  void UnloadServers();
  bool StartServers();
  void StopServers();

 private:
  typedef std::map<std::string, Service*> ServiceMap;

  std::map<std::string, ServiceProvider*> providers_;
  std::map<std::string, ServiceConfig*> configs_;

  Service* LoadService(HKEY parent, const std::string& key_name);
  Server* LoadServer(HKEY parent, const std::string& key_name);

  ServiceMap services_;
  std::vector<Server*> servers_;
};

extern ServiceManager* service_manager;

#endif  // JUNO_APP_SERVICE_MANAGER_H_
