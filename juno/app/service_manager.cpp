// Copyright (c) 2013 dacci.org

#include "app/service_manager.h"

#include "misc/registry_key.h"
#include "net/http/http_proxy.h"
#include "net/scissors/scissors.h"
#include "net/socks/socks_proxy.h"
#include "net/tcp_server.h"
#include "net/udp_server.h"

ServiceManager* service_manager = NULL;

ServiceManager::ServiceManager() {
}

ServiceManager::~ServiceManager() {
  UnloadServers();
  UnloadServices();
}

bool ServiceManager::LoadServices() {
  RegistryKey app_key;
  if (!app_key.Create(HKEY_CURRENT_USER, "Software\\dacci.org\\Juno"))
    return false;

  RegistryKey services_key;
  if (!services_key.Create(app_key, "Services"))
    return false;

  for (DWORD i = 0; ; ++i) {
    std::string name;
    if (!services_key.EnumerateKey(i, &name))
      break;

    ServiceProvider* service = LoadService(services_key, name);
    if (service != NULL)
      services_[name] = service;
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
  RegistryKey app_key;
  if (!app_key.Create(HKEY_CURRENT_USER, "Software\\dacci.org\\Juno"))
    return false;

  RegistryKey servers_key;
  if (!servers_key.Create(app_key, "Servers"))
    return false;

  for (DWORD i = 0; ; ++i) {
    std::string name;
    if (!servers_key.EnumerateKey(i, &name))
      break;

    Server* server = LoadServer(servers_key, name);
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
                                             const std::string& key_name) {
  RegistryKey reg_key;
  if (!reg_key.Open(parent, key_name))
    return NULL;

  std::string provider_name;
  if (!reg_key.QueryString("Provider", &provider_name))
    return NULL;

  ServiceProvider* provider = NULL;

  if (provider_name.compare("HttpProxy") == 0)
    provider = new HttpProxy();
  else if (provider_name.compare("SocksProxy") == 0)
    provider = new SocksProxy();
  else if (provider_name.compare("Scissors") == 0)
    provider = new Scissors();

  if (provider == NULL)
    return NULL;

  if (!provider->Setup(reg_key)) {
    delete provider;
    return NULL;
  }

  return provider;
}

Server* ServiceManager::LoadServer(HKEY parent, const std::string& key_name) {
  RegistryKey reg_key;
  if (!reg_key.Open(parent, key_name))
    return NULL;

  int enabled = reg_key.QueryInteger("Enabled");
  if (!enabled)
    return NULL;

  std::string service;
  if (!reg_key.QueryString("Service", &service))
    return NULL;

  auto service_entry = services_.find(service);
  if (service_entry == services_.end())
    return NULL;

  int listen = reg_key.QueryInteger("Listen");
  if (listen == 0)
    return NULL;

  std::string bind;
  if (!reg_key.QueryString("Bind", &bind))
    return NULL;

  int type = SOCK_STREAM;
  reg_key.QueryInteger("Type", &type);

  Server* server = NULL;
  switch (type) {
    case SOCK_STREAM:
      server = new TcpServer();
      break;

    case SOCK_DGRAM:
      server = new UdpServer();
      break;

    default:
      return NULL;
  }

  if (server == NULL)
    return NULL;

  if (!server->Setup(bind.c_str(), listen)) {
    delete server;
    return NULL;
  }

  server->SetService(service_entry->second);

  return server;
}
