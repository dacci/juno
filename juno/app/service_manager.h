// Copyright (c) 2013 dacci.org

#ifndef JUNO_APP_SERVICE_MANAGER_H_
#define JUNO_APP_SERVICE_MANAGER_H_

#include <windows.h>

#include <map>
#include <string>
#include <vector>

class ServiceProvider;
class TcpServer;

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
  typedef std::map<std::string, ServiceProvider*> ServiceMap;

  ServiceProvider* LoadService(HKEY parent, const char* key_name);
  TcpServer* LoadServer(HKEY parent, const char* key_name);

  ServiceMap services_;
  std::vector<TcpServer*> servers_;
};

#endif  // JUNO_APP_SERVICE_MANAGER_H_
