// Copyright (c) 2013 dacci.org

#include "app/service_manager.h"

#include <atlbase.h>

#include "net/http/http_proxy.h"
#include "net/socks/socks_proxy.h"
#include "net/tcp_server.h"

ServiceManager::ServiceManager() {
}

ServiceManager::~ServiceManager() {
  UnloadServers();
  UnloadServices();
}

bool ServiceManager::LoadServices() {
  CRegKey app_key;
  LONG result = app_key.Create(HKEY_CURRENT_USER, "Software\\dacci.org\\Juno");
  if (result != ERROR_SUCCESS)
    return false;

  CRegKey services_key;
  result = services_key.Create(app_key, "Services");
  if (result != ERROR_SUCCESS)
    return false;

  for (DWORD i = 0; ; ++i) {
    CString name;
    DWORD name_length = 64;
    result = services_key.EnumKey(i, name.GetBuffer(name_length),
                                  &name_length);
    if (result != ERROR_SUCCESS)
      break;

    name.ReleaseBuffer(name_length);

    ServiceProvider* service = LoadService(services_key, name);
    if (service != NULL)
      services_[name.GetString()] = service;
  }

  return true;
}

void ServiceManager::UnloadServices() {
  StopServices();

  for (auto i = services_.begin(), l = services_.end(); i != l; ++i)
    delete i->second;

  services_.clear();
}

void ServiceManager::StopServices() {
  StopServers();

  for (auto i = services_.begin(), l = services_.end(); i != l; ++i)
    i->second->Stop();
}

bool ServiceManager::LoadServers() {
  CRegKey app_key;
  LONG result = app_key.Create(HKEY_CURRENT_USER, "Software\\dacci.org\\Juno");
  if (result != ERROR_SUCCESS)
    return false;

  CRegKey servers_key;
  result = servers_key.Create(app_key, "Servers");
  if (result != ERROR_SUCCESS)
    return false;

  for (DWORD i = 0; ; ++i) {
    CString name;
    DWORD name_length = 64;
    result = servers_key.EnumKey(i, name.GetBuffer(name_length),
                                 &name_length);
    if (result != ERROR_SUCCESS)
      break;

    name.ReleaseBuffer(name_length);

    TcpServer* server = LoadServer(servers_key, name);
    if (server != NULL)
      servers_.push_back(server);
  }

  return true;
}

void ServiceManager::UnloadServers() {
  StopServers();

  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i)
    delete *i;

  servers_.clear();
}

bool ServiceManager::StartServers() {
  bool all_started = true;

  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i) {
    if (!(*i)->Start())
      all_started = false;
  }

  return all_started;
}

void ServiceManager::StopServers() {
  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i)
    (*i)->Stop();
}

ServiceProvider* ServiceManager::LoadService(HKEY parent,
                                             const char* key_name) {
  CRegKey reg_key;
  LONG result = reg_key.Open(parent, key_name);
  if (result != ERROR_SUCCESS)
    return NULL;

  CString provider_name;
  ULONG length = 64;
  result = reg_key.QueryStringValue("Provider", provider_name.GetBuffer(length),
                                    &length);
  if (result != ERROR_SUCCESS)
    return NULL;
  provider_name.ReleaseBuffer(length);

  ServiceProvider* provider = NULL;

  if (provider_name.Compare("HttpProxy") == 0)
    provider = new HttpProxy();
  else if (provider_name.Compare("SocksProxy") == 0)
    provider = new SocksProxy();

  if (provider == NULL)
    return NULL;

  if (!provider->Setup(reg_key)) {
    delete provider;
    return NULL;
  }

  return provider;
}

TcpServer* ServiceManager::LoadServer(HKEY parent, const char* key_name) {
  CRegKey reg_key;
  LONG result = reg_key.Open(parent, key_name);
  if (result != ERROR_SUCCESS)
    return NULL;

  DWORD enabled;
  result = reg_key.QueryDWORDValue("Enabled", enabled);
  if (result == ERROR_SUCCESS && !enabled)
    return NULL;

  CString service;
  ULONG length = 64;
  result = reg_key.QueryStringValue("Service", service.GetBuffer(length),
                                    &length);
  if (result != ERROR_SUCCESS)
    return NULL;

  auto service_entry = services_.find(service.GetString());
  if (service_entry == services_.end())
    return NULL;

  DWORD listen;
  result = reg_key.QueryDWORDValue("Listen", listen);
  if (result != ERROR_SUCCESS)
    return NULL;

  CString bind;
  length = 48;
  result = reg_key.QueryStringValue("Bind", bind.GetBuffer(length), &length);
  if (result != ERROR_SUCCESS)
    return NULL;
  bind.ReleaseBuffer(length);

  TcpServer* server = new TcpServer();
  if (server == NULL)
    return NULL;

  if (!server->Setup(bind, listen)) {
    delete server;
    return NULL;
  }

  server->SetService(service_entry->second);

  return server;
}
