// Copyright (c) 2013 dacci.org

#include "app/service_manager.h"

#include <assert.h>

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

  for (auto i = services_.begin(), l = services_.end(); i != l; ++i)
    delete i->second;
  services_.clear();

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

  for (auto i = services_.begin(), l = services_.end(); i != l; ++i) {
    delete i->second->instance_;
    i->second->instance_ = NULL;
  }
}

void ServiceManager::StopServices() {
  StopServers();

  for (auto i = services_.begin(), l = services_.end(); i != l; ++i)
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
    if (!i->second->server_->Start())
      all_started = false;
  }

  return all_started;
}

void ServiceManager::StopServers() {
  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i)
    if (i->second->server_ != NULL)
      i->second->server_->Stop();
}

void ServiceManager::UnloadServers() {
  StopServers();

  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i) {
    delete i->second->server_;
    i->second->server_ = NULL;
  }
}

void ServiceManager::ClearServers() {
  UnloadServers();

  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i)
    delete i->second;

  servers_.clear();
}

ServiceProvider* ServiceManager::GetProvider(const std::string& name) const {
  auto pair = providers_.find(name);
  if (pair == providers_.end())
    return NULL;

  return pair->second;
}

ServiceConfig* ServiceManager::GetServiceConfig(const std::string& name) const {
  auto pair = services_.find(name);
  if (pair == services_.end())
    return NULL;

  return pair->second;
}

void ServiceManager::CopyServiceConfigs(
    std::map<std::string, ServiceConfig*>* configs) const {
  for (auto i = services_.begin(), l = services_.end(); i != l; ++i) {
    ServiceConfig* copied = i->second->provider_->CopyConfig(i->second);
    configs->insert(std::make_pair(i->first, copied));
  }
}

void ServiceManager::CopyServerConfigs(
    std::map<std::string, ServerConfig*>* configs) const {
  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i) {
    ServerConfig* copied = new ServerConfig(*i->second);
    configs->insert(std::make_pair(i->first, copied));
  }
}

bool ServiceManager::UpdateConfiguration(
    std::map<std::string, ServiceConfig*>&& new_services,
    std::map<std::string, ServerConfig*>&& new_servers) {
  RegistryKey config_key;
  if (!config_key.Create(HKEY_CURRENT_USER, kConfigKeyName))
    return false;

  RegistryKey services_key;
  if (!services_key.Create(config_key, kServicesKeyName))
    return false;

  RegistryKey servers_key;
  if (!servers_key.Create(config_key, kServersKeyName))
    return false;

  UnloadServers();

  bool succeeded = true;

  // removed services
  for (auto i = services_.begin(), l = services_.end(); i != l;) {
    auto updated = new_services.find(i->first);
    if (updated == new_services.end()) {
      if (services_key.DeleteKey(i->first.c_str())) {
        i->second->instance_->Stop();
        delete i->second->instance_;
        delete i->second;
        services_.erase(i++);
        continue;
      } else {
        succeeded = false;
      }
    }

    ++i;
  }

  // added or updated services
  for (auto i = new_services.begin(), l = new_services.end(); i != l; ++i) {
    if (SaveService(services_key, i->second)) {
      auto existing = services_.find(i->first);
      if (existing == services_.end()) {
        // added service
        if (CreateService(i->second)) {
          services_.insert(*i);
          continue;
        } else {
          succeeded = false;
        }
      } else {
        // updated service
        if (!i->second->provider_->UpdateConfig(i->second->instance_,
                                                i->second))
          succeeded = false;
      }
    }

    delete i->second;
  }

  // removed servers
  for (auto i = servers_.begin(), l = servers_.end(); i != l;) {
    auto updated = new_servers.find(i->first);
    if (updated == new_servers.end()) {
      if (servers_key.DeleteKey(i->first.c_str())) {
        delete i->second;
        servers_.erase(i++);
        continue;
      }
    }

    ++i;
  }

  // added or updated servers
  for (auto i = new_servers.begin(), l = new_servers.end(); i != l; ++i) {
    if (SaveServer(servers_key, i->second)) {
      auto existing = servers_.find(i->first);
      if (existing == servers_.end()) {
        // added server
        servers_.insert(*i);
      } else {
        // updated server
        delete existing->second;
        existing->second = i->second;
      }
    } else {
      succeeded = false;
      delete i->second;
    }
  }

  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i)
    if (!CreateServer(i->second))
      succeeded = false;

  if (!StartServers())
    succeeded = false;

  return succeeded;
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

  services_.insert(std::make_pair(key_name, config));

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

  servers_.insert(std::make_pair(key_name, config));

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
  assert(config->server_ == NULL);

  if (!config->enabled_)
    return true;

  auto service_entry = services_.find(config->service_name_);
  if (service_entry == services_.end())
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
  config->server_ = server.release();

  return true;
}
