// Copyright (c) 2013 dacci.org

#include "app/service_manager.h"

#include <memory>

#include "app/server_config.h"
#include "app/service.h"
#include "app/service_config.h"
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
  ClearServers();
  UnloadServices();

  for (auto i = service_configs_.begin(), l = service_configs_.end(); i != l;
       ++i)
    delete i->second;
  service_configs_.clear();

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

    LoadService(services_key, name);
  }

  return true;
}

void ServiceManager::UnloadServices() {
  StopServices();

  for (auto i = service_configs_.begin(), l = service_configs_.end(); i != l;
       ++i) {
    delete i->second->instance_;
    i->second->instance_ = NULL;
  }
}

void ServiceManager::StopServices() {
  StopServers();

  for (auto i = service_configs_.begin(), l = service_configs_.end(); i != l;
       ++i)
    if (i->second->instance_ != NULL)
      i->second->instance_->Stop();
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

    LoadServer(servers_key, name);
  }

  return true;
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

void ServiceManager::UnloadServers() {
  StopServers();

  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i)
    delete *i;

  servers_.clear();
}

void ServiceManager::ClearServers() {
  UnloadServers();

  for (auto i = server_configs_.begin(), l = server_configs_.end(); i != l; ++i)
    delete i->second;

  server_configs_.clear();
}

ServiceProvider* ServiceManager::GetProvider(const std::string& name) const {
  auto pair = providers_.find(name);
  if (pair == providers_.end())
    return NULL;

  return pair->second;
}

ServiceConfig* ServiceManager::GetServiceConfig(const std::string& name) const {
  auto pair = service_configs_.find(name);
  if (pair == service_configs_.end())
    return NULL;

  return pair->second;
}

void ServiceManager::CopyServiceConfigs(
    std::map<std::string, ServiceConfig*>* configs) const {
  for (auto i = service_configs_.begin(), l = service_configs_.end(); i != l;
       ++i) {
    ServiceConfig* copied = i->second->provider_->CopyConfig(i->second);
    configs->insert(std::make_pair(i->first, copied));
  }
}

void ServiceManager::CopyServerConfigs(
    std::map<std::string, ServerConfig*>* configs) const {
  for (auto i = server_configs_.begin(), l = server_configs_.end(); i != l;
       ++i) {
    ServerConfig* copied = new ServerConfig(*i->second);
    configs->insert(std::make_pair(i->first, copied));
  }
}

bool ServiceManager::UpdateConfiguration(
    std::map<std::string, ServiceConfig*>&& new_services,
    std::map<std::string, ServerConfig*>&& new_servers) {
  UnloadServers();

  RegistryKey config_key;
  config_key.Create(HKEY_CURRENT_USER, kConfigKeyName);

  RegistryKey services_key;
  services_key.Open(config_key, kServicesKeyName);

  // removed services
  for (auto i = service_configs_.begin();;) {
    if (i == service_configs_.end())
      break;

    auto updated = new_services.find(i->first);
    if (updated == new_services.end()) {
      services_key.DeleteKey(i->first.c_str());

      i->second->instance_->Stop();
      delete i->second->instance_;
      delete i->second;
      service_configs_.erase(i++);
    } else {
      ++i;
    }
  }

  // added or updated services
  for (auto i = new_services.begin(), l = new_services.end(); i != l; ++i) {
    SaveService(services_key, i->second);

    auto existing = service_configs_.find(i->first);
    if (existing == service_configs_.end()) {
      // added service
      CreateService(i->second);
      service_configs_.insert(*i);
    } else {
      // updated service
      i->second->provider_->UpdateConfig(i->second->instance_, i->second);
      delete i->second;
    }
  }

  RegistryKey servers_key;
  servers_key.Create(config_key, kServersKeyName);

  // removed servers
  for (auto i = server_configs_.begin();;) {
    if (i == server_configs_.end())
      break;

    auto updated = new_servers.find(i->first);
    if (updated == new_servers.end()) {
      servers_key.DeleteKey(i->first.c_str());

      delete i->second;
      server_configs_.erase(i++);
    } else {
      ++i;
    }
  }

  // added or updated servers
  for (auto i = new_servers.begin(), l = new_servers.end(); i != l; ++i) {
    SaveServer(servers_key, i->second);

    auto existing = server_configs_.find(i->first);
    if (existing == server_configs_.end()) {
      server_configs_.insert(*i);
    } else {
      delete existing->second;
      existing->second = i->second;
    }
  }

  for (auto i = server_configs_.begin(), l = server_configs_.end(); i != l; ++i)
    CreateServer(i->second);

  StartServers();

  return true;
}

bool ServiceManager::LoadService(const RegistryKey& parent,
                                 const std::string& key_name) {
  RegistryKey reg_key;
  if (!reg_key.Open(parent, key_name))
    return false;

  std::string provider_name;
  if (!reg_key.QueryString(kProviderValueName, &provider_name))
    return false;

  ServiceProvider* provider = GetProvider(provider_name);
  if (provider == NULL)
    return false;

  ServiceConfig* config = provider->LoadConfig(reg_key);
  if (config == NULL)
    return false;

  config->name_ = key_name;
  config->provider_name_ = provider_name;
  config->provider_ = provider;

  service_configs_.insert(std::make_pair(key_name, config));

  return CreateService(config);
}

bool ServiceManager::SaveService(const RegistryKey& parent,
                                 ServiceConfig* config) {
  RegistryKey service_key;
  if (!service_key.Create(parent, config->name_))
    return false;

  if (!service_key.SetString(kProviderValueName, config->provider_name_))
    return false;

  return config->provider_->SaveConfig(config, &service_key);
}

bool ServiceManager::CreateService(ServiceConfig* config) {
  config->instance_ = config->provider_->CreateService(config);
  return config->instance_ != NULL;
}

bool ServiceManager::LoadServer(const RegistryKey& parent,
                                const std::string& key_name) {
  RegistryKey reg_key;
  if (!reg_key.Open(parent, key_name))
    return false;

  ServerConfig* config = new ServerConfig();
  config->name_ = key_name;

  reg_key.QueryString(kBindValueName, &config->bind_);
  reg_key.QueryInteger(kListenValueName, &config->listen_);
  reg_key.QueryInteger(kTypeValueName, &config->type_);
  reg_key.QueryString(kServiceValueName, &config->service_name_);
  reg_key.QueryInteger(kEnabledValueName, &config->enabled_);

  server_configs_.insert(std::make_pair(key_name, config));

  return CreateServer(config);
}

bool ServiceManager::SaveServer(const RegistryKey& parent,
                                ServerConfig* config) {
  RegistryKey key;
  if (!key.Create(parent, config->name_))
    return false;

  key.SetString(kBindValueName, config->bind_);
  key.SetInteger(kListenValueName, config->listen_);
  key.SetInteger(kTypeValueName, config->type_);
  key.SetString(kServiceValueName, config->service_name_);
  key.SetInteger(kEnabledValueName, config->enabled_);

  return true;
}

bool ServiceManager::CreateServer(ServerConfig* config) {
  if (!config->enabled_)
    return true;

  auto service_entry = service_configs_.find(config->service_name_);
  if (service_entry == service_configs_.end())
    return false;

  std::unique_ptr<Server> server;
  switch (config->type_) {
    case SOCK_STREAM:
      server.reset(new TcpServer());
      break;

    case SOCK_DGRAM:
      server.reset(new UdpServer());
      break;

    default:
      return false;
  }

  if (server == NULL)
    return false;

  if (!server->Setup(config->bind_.c_str(), config->listen_))
    return false;

  server->SetService(service_entry->second->instance_);
  servers_.push_back(server.release());

  return true;
}
