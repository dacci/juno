// Copyright (c) 2013 dacci.org

#include "app/service_manager.h"

#include "app/service.h"
#include "misc/registry_key-inl.h"
#include "net/http/http_proxy_provider.h"
#include "net/scissors/scissors_provider.h"
#include "net/socks/socks_proxy_provider.h"
#include "net/tcp_server.h"
#include "net/udp_server.h"

ServiceManager* service_manager = NULL;

namespace {
const char kConfigKeyName[] = "Software\\dacci.org\\Juno";
const char kServicesKeyName[] = "Services";
const char kServersKeyName[] = "Servers";
const char kProviderValueName[] = "Provider";
const char kEnabledValueName[] = "Enabled";
const char kServiceValueName[] = "Service";
const char kListenValueName[] = "Listen";
const char kBindValueName[] = "Bind";
const char kTypeValueName[] = "Type";
}  // namespace

ServiceManager::ServiceManager() {
#define PROVIDER_ENTRY(key) \
  providers_.insert(std::make_pair(#key, new key##Provider))

  PROVIDER_ENTRY(HttpProxy);
  PROVIDER_ENTRY(SocksProxy);
  PROVIDER_ENTRY(Scissors);

#undef PROVIDER_ENTRY
}

ServiceManager::~ServiceManager() {
  UnloadServers();
  UnloadServices();

  for (auto i = configs_.begin(), l = configs_.end(); i != l; ++i)
    delete i->second;
  configs_.clear();

  for (auto i = providers_.begin(), l = providers_.end(); i != l; ++i)
    delete i->second;
  providers_.clear();
}

bool ServiceManager::LoadServices() {
  RegistryKey app_key;
  if (!app_key.Create(HKEY_CURRENT_USER, kConfigKeyName))
    return false;

  RegistryKey services_key;
  if (!services_key.Create(app_key, kServicesKeyName))
    return false;

  for (DWORD i = 0; ; ++i) {
    std::string name;
    if (!services_key.EnumerateKey(i, &name))
      break;

    Service* service = LoadService(services_key, name);
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
  if (!app_key.Create(HKEY_CURRENT_USER, kConfigKeyName))
    return false;

  RegistryKey servers_key;
  if (!servers_key.Create(app_key, kServersKeyName))
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

Service* ServiceManager::LoadService(HKEY parent, const std::string& key_name) {
  RegistryKey reg_key;
  if (!reg_key.Open(parent, key_name))
    return NULL;

  std::string provider_name;
  if (!reg_key.QueryString(kProviderValueName, &provider_name))
    return NULL;

  auto pair = providers_.find(provider_name);
  if (pair == providers_.end())
    return NULL;

  ServiceConfig* config = pair->second->LoadConfig(reg_key);
  if (config == NULL)
    return NULL;

  configs_.insert(std::make_pair(key_name, config));

  return pair->second->CreateService(config);
}

Server* ServiceManager::LoadServer(HKEY parent, const std::string& key_name) {
  RegistryKey reg_key;
  if (!reg_key.Open(parent, key_name))
    return NULL;

  int enabled = reg_key.QueryInteger(kEnabledValueName);
  if (!enabled)
    return NULL;

  std::string service;
  if (!reg_key.QueryString(kServiceValueName, &service))
    return NULL;

  auto service_entry = services_.find(service);
  if (service_entry == services_.end())
    return NULL;

  int listen = reg_key.QueryInteger(kListenValueName);
  if (listen == 0)
    return NULL;

  std::string bind;
  if (!reg_key.QueryString(kBindValueName, &bind))
    return NULL;

  int type = SOCK_STREAM;
  reg_key.QueryInteger(kTypeValueName, &type);

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
