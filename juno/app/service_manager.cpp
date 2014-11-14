// Copyright (c) 2013 dacci.org

#include "app/service_manager.h"

#include <assert.h>

#include <memory>
#include <vector>

#include "misc/registry_key-inl.h"
#include "misc/security/certificate_store.h"
#include "net/secure_socket_channel.h"
#include "net/tcp_server.h"
#include "net/udp_server.h"
#include "service/service.h"
#include "service/http/http_proxy_provider.h"
#include "service/scissors/scissors_provider.h"
#include "service/socks/socks_proxy_provider.h"

ServiceManager* service_manager = nullptr;

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
const char kCertificateValueName[] = "Certificate";

class SecureChannelFactory : public TcpServer::ChannelFactory {
 public:
  Service::ChannelPtr CreateChannel(
      const std::shared_ptr<madoka::net::AsyncSocket>& socket) {
    return std::make_shared<SecureSocketChannel>(&credential_, socket, true);
  }

  SchannelCredential credential_;
};

CertificateStore certificate_store(L"MY");
std::vector<std::unique_ptr<SecureChannelFactory>> channel_factories;

}  // namespace

ServiceManager::ServiceManager() {
#define PROVIDER_ENTRY(key) \
  providers_.insert(std::make_pair(#key, std::make_shared<key##Provider>()))

  PROVIDER_ENTRY(HttpProxy);
  PROVIDER_ENTRY(SocksProxy);
  PROVIDER_ENTRY(Scissors);

#undef PROVIDER_ENTRY
}

ServiceManager::~ServiceManager() {
  StopServers();
  StopServices();

#ifdef _DEBUG
  for (auto& pair : server_configs_) {
    if (!pair.second.unique())
      abort();
  }

  for (auto& pair : service_configs_) {
    if (!pair.second.unique())
      abort();
  }

  for (auto& pair : providers_) {
    if (!pair.second.unique())
      abort();
  }
#endif  // _DEBUG
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

void ServiceManager::StopServices() {
  StopServers();

  for (auto& pair : services_)
    pair.second->Stop();
  services_.clear();
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

  for (auto& pair : servers_) {
    if (!pair.second->Start())
      all_started = false;
  }

  return all_started;
}

void ServiceManager::StopServers() {
  for (auto& pair : servers_)
    pair.second->Stop();
  servers_.clear();

  channel_factories.clear();
}

ServiceProviderPtr ServiceManager::GetProvider(const std::string& name) const {
  auto& pair = providers_.find(name);
  if (pair == providers_.end())
    return nullptr;

  return pair->second;
}

ServiceConfigPtr ServiceManager::GetServiceConfig(
    const std::string& name) const {
  auto& pair = service_configs_.find(name);
  if (pair == service_configs_.end())
    return nullptr;

  return pair->second;
}

void ServiceManager::CopyServiceConfigs(ServiceConfigMap* configs) const {
  for (auto& pair : service_configs_) {
    auto& copy = providers_.at(pair.second->provider_name_)->
        CopyConfig(pair.second);
    configs->insert(std::make_pair(pair.first, std::move(copy)));
  }
}

void ServiceManager::CopyServerConfigs(ServerConfigMap* configs) const {
  for (auto& pair : server_configs_) {
    auto& copy = std::make_shared<ServerConfig>(*pair.second);
    configs->insert(std::make_pair(pair.first, std::move(copy)));
  }
}

bool ServiceManager::UpdateConfiguration(ServiceConfigMap&& new_services,
                                         ServerConfigMap&& new_servers) {
  RegistryKey config_key;
  if (!config_key.Create(HKEY_CURRENT_USER, kConfigKeyName))
    return false;

  RegistryKey services_key;
  if (!services_key.Create(config_key, kServicesKeyName))
    return false;

  RegistryKey servers_key;
  if (!servers_key.Create(config_key, kServersKeyName))
    return false;

  StopServers();

  bool succeeded = true;

  // removed services
  for (auto i = service_configs_.begin(), l = service_configs_.end(); i != l;) {
    auto& updated = new_services.find(i->first);
    if (updated == new_services.end()) {
      if (services_key.DeleteKey(i->first.c_str())) {
        services_.at(i->first)->Stop();
        services_.erase(i->first);

        service_configs_.erase(i++);
        continue;
      } else {
        succeeded = false;
      }
    }

    ++i;
  }

  // added or updated services
  for (auto i = new_services.begin(), l = new_services.end(); i != l; ++i) {
    if (!SaveService(services_key, i->second))
      continue;

    auto& existing = service_configs_.find(i->first);
    if (existing == service_configs_.end()) {
      // added service
      service_configs_.insert(*i);

      if (!CreateService(i->first))
        succeeded = false;
    } else {
      // updated service
      existing->second = i->second;

      if (!services_.at(i->first)->UpdateConfig(i->second))
        succeeded = false;
    }
  }

  // removed servers
  for (auto i = server_configs_.begin(), l = server_configs_.end(); i != l;) {
    auto updated = new_servers.find(i->first);
    if (updated == new_servers.end()) {
      if (servers_key.DeleteKey(i->first.c_str())) {
        server_configs_.erase(i++);
        continue;
      }
    }

    ++i;
  }

  // added or updated servers
  for (auto i = new_servers.begin(), l = new_servers.end(); i != l; ++i) {
    if (SaveServer(servers_key, i->second)) {
      auto existing = server_configs_.find(i->first);
      if (existing == server_configs_.end()) {
        // added server
        server_configs_.insert(*i);
      } else {
        // updated server
        existing->second = std::move(i->second);
      }
    } else {
      succeeded = false;
    }
  }

  for (auto& pair : server_configs_)
    if (!CreateServer(pair.first))
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

  ServiceProviderPtr provider = GetProvider(provider_name);
  if (provider == nullptr)
    return false;

  ServiceConfigPtr config = provider->LoadConfig(reg_key);
  if (config == nullptr)
    return false;

  config->name_ = key_name;
  config->provider_name_ = provider_name;

  service_configs_.insert(std::make_pair(key_name, config));

  return CreateService(key_name);
}

bool ServiceManager::SaveService(const RegistryKey& parent,
                                 const ServiceConfigPtr& config) {
  RegistryKey service_key;
  if (!service_key.Create(parent, config->name_))
    return false;

  if (!service_key.SetString(kProviderValueName, config->provider_name_))
    return false;

  return providers_.at(config->provider_name_)->
      SaveConfig(config, &service_key);
}

bool ServiceManager::CreateService(const std::string& name) {
  assert(service_configs_.find(name) != service_configs_.end());
  auto& config = service_configs_.at(name);
  auto& service = providers_.at(config->provider_name_)->CreateService(config);
  if (service == nullptr)
    return false;

  services_.insert(std::make_pair(name, std::move(service)));

  return true;
}

bool ServiceManager::LoadServer(const RegistryKey& parent,
                                const std::string& key_name) {
  RegistryKey reg_key;
  if (!reg_key.Open(parent, key_name))
    return false;

  ServerConfigPtr config(new ServerConfig());
  config->name_ = key_name;

  reg_key.QueryString(kBindValueName, &config->bind_);
  reg_key.QueryInteger(kListenValueName, &config->listen_);
  reg_key.QueryInteger(kTypeValueName, &config->type_);
  reg_key.QueryString(kServiceValueName, &config->service_name_);
  reg_key.QueryInteger(kEnabledValueName, &config->enabled_);

  int length = 20;
  config->cert_hash_.resize(length);
  reg_key.QueryBinary(kCertificateValueName, config->cert_hash_.data(),
                      &length);
  config->cert_hash_.resize(length);

  server_configs_.insert(std::make_pair(key_name, config));

  return CreateServer(key_name);
}

bool ServiceManager::SaveServer(const RegistryKey& parent,
                                const ServerConfigPtr& config) {
  RegistryKey key;
  if (!key.Create(parent, config->name_))
    return false;

  key.SetString(kBindValueName, config->bind_);
  key.SetInteger(kListenValueName, config->listen_);
  key.SetInteger(kTypeValueName, config->type_);
  key.SetString(kServiceValueName, config->service_name_);
  key.SetInteger(kEnabledValueName, config->enabled_);
  key.SetBinary(kCertificateValueName, config->cert_hash_.data(),
                config->cert_hash_.size());

  return true;
}

bool ServiceManager::CreateServer(const std::string& name) {
  assert(server_configs_.find(name) != server_configs_.end());

  auto& config = server_configs_.at(name);
  assert(config != nullptr);

  if (!config->enabled_)
    return true;

  auto& service = services_.find(config->service_name_);
  if (service == services_.end())
    return false;

  ServerPtr server;
  switch (config->type_) {
    case ServerConfig::TCP:
      server.reset(new TcpServer());
      break;

    case ServerConfig::UDP:
      server.reset(new UdpServer());
      break;

    case ServerConfig::TLS: {
      std::unique_ptr<SecureChannelFactory> factory(new SecureChannelFactory());
      if (factory == nullptr)
        break;

      auto& credential = factory->credential_;
      credential.SetEnabledProtocols(SP_PROT_SSL3TLS1_X_SERVERS);
      credential.SetFlags(SCH_CRED_MANUAL_CRED_VALIDATION);

      CRYPT_HASH_BLOB hash_blob = {
        config->cert_hash_.size(),
        config->cert_hash_.data()
      };
      PCCERT_CONTEXT cert = certificate_store.FindCertificate(
          X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_HASH,
          &hash_blob, nullptr);
      if (cert == nullptr)
        break;

      credential.AddCertificate(cert);
      CertFreeCertificateContext(cert);
      cert = nullptr;

      if (FAILED(credential.AcquireHandle(SECPKG_CRED_INBOUND)))
        break;

      server.reset(new TcpServer());
      if (server == nullptr)
        break;

      TcpServer* tcp_server = static_cast<TcpServer*>(server.get());
      tcp_server->SetChannelFactory(factory.get());

      channel_factories.push_back(std::move(factory));

      break;
    }

    default:
      return false;
  }

  if (server == nullptr)
    return false;

  if (!server->Setup(config->bind_.c_str(), config->listen_))
    return false;

  server->SetService(service->second.get());

  servers_.insert(std::make_pair(name, std::move(server)));

  return true;
}
