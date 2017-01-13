// Copyright (c) 2013 dacci.org

#include "service/service_manager.h"

#include <base/logging.h>
#include <base/strings/sys_string_conversions.h>

#include <memory>
#include <string>
#include <vector>

#include "app/application.h"
#include "app/constants.h"
#include "io/secure_channel.h"
#include "misc/certificate_store.h"
#include "misc/registry_key-inl.h"
#include "misc/string_util.h"
#include "service/rpc/rpc_common.h"
#include "service/rpc/rpc_service.h"
#include "service/service.h"
#include "service/tcp_server.h"
#include "service/udp_server.h"

#include "service/http/http_proxy_provider.h"
#include "service/scissors/scissors_provider.h"
#include "service/socks/socks_proxy_provider.h"

namespace juno {
namespace service {
namespace {

const char kConfigKeyName[] = "Software\\dacci.org\\Juno";
const char kServicesKeyName[] = "Services";
const char kServersKeyName[] = "Servers";

const wchar_t kNameValueName[] = L"Name";
const char kProviderValueName[] = "Provider";
const char kBindValueName[] = "Bind";
const char kListenValueName[] = "Listen";
const char kTypeValueName[] = "Type";
const char kServiceValueName[] = "Service";
const char kEnabledValueName[] = "Enabled";
const char kCertificateValueName[] = "Certificate";

const char kIdJson[] = "id";
const char kNameJson[] = "name";
const char kProviderJson[] = "provider";
const char kBindJson[] = "bind";
const char kListenJson[] = "listen";
const char kTypeJson[] = "type";
const char kServiceJson[] = "service";
const char kEnabledJson[] = "enabled";
const char kCertificateJson[] = "certificate";

class SecureChannelCustomizer : public TcpServer::ChannelCustomizer {
 public:
  io::ChannelPtr Customize(const io::ChannelPtr& channel) override {
    return std::make_shared<io::SecureChannel>(&credential_, channel, true);
  }

  misc::schannel::SchannelCredential credential_;
};

misc::CertificateStore certificate_store(L"MY");
std::vector<std::unique_ptr<SecureChannelCustomizer>> channel_customizers;

}  // namespace

using ::juno::misc::RegistryKey;

ServiceManager* ServiceManager::instance_ = nullptr;

ServiceManager::ServiceManager() : root_key_(NULL) {
  DCHECK(instance_ == nullptr);
  instance_ = this;

  root_key_ = app::GetApplication()->IsService() ? HKEY_LOCAL_MACHINE
                                                 : HKEY_CURRENT_USER;

#define PROVIDER_ENTRY(key, ns) \
  providers_.insert({#key, std::make_unique<ns::key##Provider>()})

  PROVIDER_ENTRY(HttpProxy, service::http);
  PROVIDER_ENTRY(SocksProxy, service::socks);
  PROVIDER_ENTRY(Scissors, service::scissors);

#undef PROVIDER_ENTRY

  auto rpc_service = app::GetApplication()->GetRpcService();
  if (rpc_service != nullptr) {
    rpc_service->RegisterMethod(kConfigGetMethod, GetConfig, this);
    rpc_service->RegisterMethod(kConfigSetMethod, SetConfig, this);
  }
}

ServiceManager::~ServiceManager() {
  StopServers();
  StopServices();

  auto rpc_service = app::GetApplication()->GetRpcService();
  if (rpc_service != nullptr) {
    rpc_service->UnregisterMethod(kConfigGetMethod);
    rpc_service->UnregisterMethod(kConfigSetMethod);
  }
}

bool ServiceManager::LoadServices() {
  RegistryKey app_key;
  if (!app_key.Create(root_key_, kConfigKeyName))
    return false;

  RegistryKey services_key;
  if (!services_key.Create(app_key, kServicesKeyName))
    return false;

  for (DWORD i = 0;; ++i) {
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
  if (!app_key.Create(root_key_, kConfigKeyName))
    return false;

  RegistryKey servers_key;
  if (!servers_key.Create(app_key, kServersKeyName))
    return false;

  auto all_succeeded = true;

  for (DWORD i = 0;; ++i) {
    std::string id;
    if (!servers_key.EnumerateKey(i, &id))
      break;

    if (!LoadServer(servers_key, id))
      all_succeeded = false;
  }

  return all_succeeded;
}

bool ServiceManager::StartServers() {
  auto all_started = true;

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

  channel_customizers.clear();
}

ServiceProvider* ServiceManager::GetProvider(const std::string& name) const {
  auto pair = providers_.find(name);
  if (pair == providers_.end())
    return nullptr;

  return pair->second.get();
}

void ServiceManager::CopyServiceConfigs(ServiceConfigMap* configs) const {
  for (auto& pair : service_configs_) {
    auto copy =
        providers_.at(pair.second->provider_)->CopyConfig(pair.second.get());
    configs->insert({pair.first, std::move(copy)});
  }
}

void ServiceManager::CopyServerConfigs(ServerConfigMap* configs) const {
  for (auto& pair : server_configs_) {
    auto copy = std::make_unique<ServerConfig>(*pair.second);
    configs->insert({pair.first, std::move(copy)});
  }
}

bool ServiceManager::UpdateConfiguration(
    ServiceConfigMap&& new_services,  // NOLINT(whitespace/operators)
    ServerConfigMap&& new_servers) {  // NOLINT(whitespace/operators)
  if (SHDeleteKeyA(root_key_, kConfigKeyName) != ERROR_SUCCESS)
    return false;

  RegistryKey config_key;
  if (!config_key.Create(root_key_, kConfigKeyName))
    return false;

  RegistryKey services_key;
  if (!services_key.Create(config_key, kServicesKeyName))
    return false;

  RegistryKey servers_key;
  if (!servers_key.Create(config_key, kServersKeyName))
    return false;

  StopServers();

  auto succeeded = true;

  // removed services
  for (auto i = service_configs_.begin(), l = service_configs_.end(); i != l;) {
    auto updated = new_services.find(i->first);
    if (updated == new_services.end() ||
        service_configs_[i->first]->provider_ != updated->second->provider_) {
      services_[i->first]->Stop();
      services_.erase(i->first);

      service_configs_.erase(i++);
    } else {
      ++i;
    }
  }

  // added or updated services
  for (auto i = new_services.begin(), l = new_services.end(); i != l; ++i) {
    if (!SaveService(services_key, i->second.get()))
      continue;

    auto existing = service_configs_.find(i->first);
    if (existing == service_configs_.end()) {
      // added service
      service_configs_.insert({i->first, std::move(i->second)});

      if (!CreateService(i->first))
        succeeded = false;
    } else {
      // updated service
      if (services_[i->first]->UpdateConfig(i->second.get()))
        existing->second = std::move(i->second);
      else
        succeeded = false;
    }
  }

  // removed servers
  for (auto i = server_configs_.begin(), l = server_configs_.end(); i != l;) {
    auto updated = new_servers.find(i->first);
    if (updated == new_servers.end())
      server_configs_.erase(i++);
    else
      ++i;
  }

  // added or updated servers
  for (auto i = new_servers.begin(), l = new_servers.end(); i != l; ++i) {
    if (SaveServer(servers_key, i->second.get())) {
      auto existing = server_configs_.find(i->first);
      if (existing == server_configs_.end()) {
        // added server
        server_configs_.insert({i->first, std::move(i->second)});
      } else {
        // updated server
        existing->second = std::move(i->second);
      }
    } else {
      succeeded = false;
    }
  }

  for (auto& pair : server_configs_) {
    if (!CreateServer(pair.first))
      succeeded = false;
  }

  if (!StartServers())
    succeeded = false;

  return succeeded;
}

std::unique_ptr<base::ListValue> ServiceManager::ConvertServiceConfig(
    const ServiceConfigMap* configs) const {
  if (configs == nullptr)
    return nullptr;

  auto list = std::make_unique<base::ListValue>();
  if (list == nullptr)
    return nullptr;

  for (const auto& pair : *configs) {
    auto value = ConvertServiceConfig(pair.second.get());
    if (value == nullptr)
      return nullptr;

    list->Append(std::move(value));
  }

  return std::move(list);
}

std::unique_ptr<base::DictionaryValue> ServiceManager::ConvertServiceConfig(
    const ServiceConfig* config) const {
  if (config == nullptr)
    return nullptr;

  auto provider = GetProvider(config->provider_);
  if (provider == nullptr)
    return nullptr;

  auto value = provider->ConvertConfig(config);
  if (value == nullptr)
    return nullptr;

  value->SetString(kIdJson, config->id_);
  value->SetString(kNameJson, config->name_);
  value->SetString(kProviderJson, config->provider_);

  return std::move(value);
}

bool ServiceManager::ConvertServiceConfig(const base::ListValue* values,
                                          ServiceConfigMap* output) const {
  if (values == nullptr || output == nullptr)
    return false;

  output->clear();

  for (size_t i = 0, l = values->GetSize(); i < l; ++i) {
    const base::DictionaryValue* value;
    if (!values->GetDictionary(i, &value))
      return false;

    auto config = ConvertServiceConfig(value);
    if (config == nullptr)
      return false;

    output->insert({config->id_, std::move(config)});
  }

  return true;
}

std::unique_ptr<ServiceConfig> ServiceManager::ConvertServiceConfig(
    const base::DictionaryValue* value) const {
  if (value == nullptr)
    return nullptr;

  std::string provider_name;
  if (!value->GetString(kProviderJson, &provider_name))
    return nullptr;

  auto provider = GetProvider(provider_name);
  if (provider == nullptr)
    return nullptr;

  auto config = provider->ConvertConfig(value);
  if (config == nullptr)
    return nullptr;

  if (!value->GetString(kIdJson, &config->id_) ||
      !value->GetString(kNameJson, &config->name_))
    return nullptr;

  config->provider_ = std::move(provider_name);

  return std::move(config);
}

std::unique_ptr<base::ListValue> ServiceManager::ConvertServerConfig(
    const ServerConfigMap* configs) {
  if (configs == nullptr)
    return nullptr;

  auto list = std::make_unique<base::ListValue>();
  if (list == nullptr)
    return nullptr;

  for (const auto& pair : *configs) {
    auto value = ConvertServerConfig(pair.second.get());
    if (value == nullptr)
      return nullptr;

    list->Append(std::move(value));
  }

  return std::move(list);
}

std::unique_ptr<base::DictionaryValue> ServiceManager::ConvertServerConfig(
    const ServerConfig* config) {
  if (config == nullptr)
    return nullptr;

  auto value = std::make_unique<base::DictionaryValue>();
  if (value == nullptr)
    return nullptr;

  value->SetString(kIdJson, config->id_);
  value->SetString(kBindJson, config->bind_);
  value->SetInteger(kListenJson, config->listen_);
  value->SetInteger(kTypeJson, config->type_);
  value->SetString(kServiceJson, config->service_);
  value->SetInteger(kEnabledJson, config->enabled_);

  if (!config->cert_hash_.empty()) {
    auto cert_hash = base::BinaryValue::CreateWithCopiedBuffer(
        config->cert_hash_.data(), config->cert_hash_.size());
    if (cert_hash != nullptr)
      value->Set(kCertificateJson, std::move(cert_hash));
  }

  return std::move(value);
}

bool ServiceManager::ConvertServerConfig(const base::ListValue* values,
                                         ServerConfigMap* output) {
  if (values == nullptr || output == nullptr)
    return false;

  for (size_t i = 0, l = values->GetSize(); i < l; ++i) {
    const base::DictionaryValue* value;
    if (!values->GetDictionary(i, &value))
      return false;

    auto config = ConvertServerConfig(value);
    if (config == nullptr)
      return false;

    output->insert({config->id_, std::move(config)});
  }

  return true;
}

std::unique_ptr<ServerConfig> ServiceManager::ConvertServerConfig(
    const base::DictionaryValue* value) {
  if (value == nullptr)
    return nullptr;

  auto config = std::make_unique<ServerConfig>();
  if (config == nullptr)
    return nullptr;

  value->GetString(kIdJson, &config->id_);
  value->GetString(kBindJson, &config->bind_);
  value->GetInteger(kListenJson, &config->listen_);
  value->GetInteger(kTypeJson, &config->type_);
  value->GetString(kServiceJson, &config->service_);
  value->GetInteger(kEnabledJson, &config->enabled_);

  const base::BinaryValue* cert_hash;
  if (value->GetBinary(kCertificateJson, &cert_hash) &&
      cert_hash->GetSize() > 0)
    config->cert_hash_.assign(cert_hash->GetBuffer(), cert_hash->GetSize());

  return std::move(config);
}

bool ServiceManager::LoadService(const RegistryKey& parent,
                                 const std::string& id) {
  RegistryKey reg_key;
  if (!reg_key.Open(parent, id))
    return false;

  std::string provider_name;
  if (!reg_key.QueryString(kProviderValueName, &provider_name))
    return false;

  auto provider = GetProvider(provider_name);
  if (provider == nullptr)
    return false;

  auto config = provider->LoadConfig(reg_key);
  if (config == nullptr)
    return false;

  // convert name to GUID
  if (reg_key.QueryString(kNameValueName, &config->name_)) {
    config->id_ = id;
  } else {
    config->id_ = misc::GenerateGUID();
    config->name_ = base::SysNativeMBToWide(id);
  }

  config->provider_ = provider_name;

  auto service_id = config->id_;
  service_configs_.insert({service_id, std::move(config)});

  return CreateService(service_id);
}

bool ServiceManager::SaveService(const RegistryKey& parent,
                                 const ServiceConfig* config) {
  RegistryKey service_key;
  if (!service_key.Create(parent, config->id_))
    return false;

  if (!service_key.SetString(kNameValueName, config->name_) ||
      !service_key.SetString(kProviderValueName, config->provider_))
    return false;

  return providers_[config->provider_]->SaveConfig(config, &service_key);
}

bool ServiceManager::CreateService(const std::string& id) {
  DCHECK(service_configs_.find(id) != service_configs_.end());

  auto& config = service_configs_[id];
  auto service = providers_[config->provider_]->CreateService(config.get());
  if (service == nullptr)
    return false;

  services_.insert({id, std::move(service)});

  return true;
}

bool ServiceManager::LoadServer(const RegistryKey& parent,
                                const std::string& id) {
  RegistryKey reg_key;
  if (!reg_key.Open(parent, id))
    return false;

  auto config = std::make_unique<ServerConfig>();
  config->id_ = id;

  reg_key.QueryString(kBindValueName, &config->bind_);
  reg_key.QueryInteger(kListenValueName, &config->listen_);
  reg_key.QueryInteger(kTypeValueName, &config->type_);
  reg_key.QueryString(kServiceValueName, &config->service_);
  reg_key.QueryInteger(kEnabledValueName, &config->enabled_);

  // convert name to GUID
  if (service_configs_.find(config->service_) == service_configs_.end()) {
    auto service = base::SysNativeMBToWide(config->service_);
    for (const auto& pair : service_configs_) {
      if (pair.second->name_ == service) {
        config->service_ = pair.second->id_;
        break;
      }
    }
  }

  auto length = 20;
  config->cert_hash_.resize(length);
  reg_key.QueryBinary(kCertificateValueName, &config->cert_hash_[0], &length);
  config->cert_hash_.resize(length);

  server_configs_.insert({id, std::move(config)});

  return CreateServer(id);
}

bool ServiceManager::CreateServer(const std::string& id) {
  DCHECK(server_configs_.find(id) != server_configs_.end());

  auto& config = server_configs_[id];
  DCHECK(config != nullptr);

  if (!config->enabled_)
    return true;

  auto service = services_.find(config->service_);
  if (service == services_.end())
    return false;

  std::unique_ptr<Server> server;
  switch (static_cast<ServerConfig::Protocol>(config->type_)) {
    case ServerConfig::Protocol::kTCP:
      server = std::make_unique<TcpServer>();
      break;

    case ServerConfig::Protocol::kUDP:
      server = std::make_unique<UdpServer>();
      break;

    case ServerConfig::Protocol::kTLS: {
      auto factory = std::make_unique<SecureChannelCustomizer>();
      if (factory == nullptr)
        break;

      auto& credential = factory->credential_;
      credential.SetEnabledProtocols(SP_PROT_SSL3TLS1_X_SERVERS);
      credential.SetFlags(SCH_CRED_MANUAL_CRED_VALIDATION);

      CRYPT_HASH_BLOB hash_blob{
          static_cast<DWORD>(config->cert_hash_.size()),
          reinterpret_cast<BYTE*>(&config->cert_hash_[0])};
      auto cert = certificate_store.FindCertificate(
          X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_HASH,
          &hash_blob, nullptr);
      if (cert == nullptr)
        break;

      credential.AddCertificate(cert);
      CertFreeCertificateContext(cert);
      cert = nullptr;

      if (FAILED(credential.AcquireHandle(SECPKG_CRED_INBOUND)))
        break;

      auto tcp_server = std::make_unique<TcpServer>();
      if (tcp_server != nullptr) {
        tcp_server->SetChannelCustomizer(factory.get());

        channel_customizers.push_back(std::move(factory));
        server = std::move(tcp_server);
      }
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

  servers_.insert({id, std::move(server)});

  return true;
}

bool ServiceManager::SaveServer(const RegistryKey& parent,
                                const ServerConfig* config) {
  RegistryKey key;
  if (!key.Create(parent, config->id_))
    return false;

  key.SetString(kBindValueName, config->bind_);
  key.SetInteger(kListenValueName, config->listen_);
  key.SetInteger(kTypeValueName, config->type_);
  key.SetString(kServiceValueName, config->service_);
  key.SetInteger(kEnabledValueName, config->enabled_);
  key.SetBinary(kCertificateValueName, config->cert_hash_.data(),
                static_cast<int>(config->cert_hash_.size()));

  return true;
}

void ServiceManager::GetConfig(void* context, const base::Value* /*params*/,
                               base::DictionaryValue* response) {
  if (response == nullptr) {
    LOG(ERROR) << "Method called as notification.";
    return;
  }

  auto manager = static_cast<const ServiceManager*>(context);
  HRESULT result;

  do {
    auto services = manager->ConvertServiceConfig(&manager->service_configs_);
    auto servers = ConvertServerConfig(&manager->server_configs_);
    if (services == nullptr || servers == nullptr) {
      result = E_OUTOFMEMORY;
      break;
    }

    response->Set("result.services", std::move(services));
    response->Set("result.servers", std::move(servers));

    return;
  } while (false);

  response->SetInteger(rpc::properties::kErrorCode, rpc::codes::kServerError);
  response->SetString(rpc::properties::kErrorMessage,
                      rpc::messages::kServerError);
  response->SetInteger(rpc::properties::kErrorData, result);
}

void ServiceManager::SetConfig(void* context, const base::Value* params,
                               base::DictionaryValue* response) {
  if (response == nullptr) {
    LOG(ERROR) << "Method called as notification.";
    return;
  }

  if (params == nullptr || !params->IsType(base::Value::TYPE_DICTIONARY)) {
    response->SetInteger(rpc::properties::kErrorCode,
                         rpc::codes::kInvalidParams);
    response->SetString(rpc::properties::kErrorMessage,
                        rpc::messages::kInvalidParams);
    return;
  }

  const base::DictionaryValue* object;
  params->GetAsDictionary(&object);

  auto manager = static_cast<ServiceManager*>(context);
  HRESULT result;

  do {
    const base::ListValue* services = nullptr;
    const base::ListValue* servers = nullptr;
    if (!object->GetList("services", &services) ||
        !object->GetList("servers", &servers)) {
      result = E_INVALIDARG;
      break;
    }

    ServiceConfigMap new_services;
    ServerConfigMap new_servers;
    if (!manager->ConvertServiceConfig(services, &new_services) ||
        !ConvertServerConfig(servers, &new_servers)) {
      result = E_UNEXPECTED;
      break;
    }

    auto succeeded = manager->UpdateConfiguration(std::move(new_services),
                                                  std::move(new_servers));
    response->SetBoolean(rpc::properties::kResult, succeeded);

    return;
  } while (false);

  response->SetInteger(rpc::properties::kErrorCode, rpc::codes::kServerError);
  response->SetString(rpc::properties::kErrorMessage,
                      rpc::messages::kServerError);
  response->SetInteger(rpc::properties::kErrorData, result);
}

}  // namespace service
}  // namespace juno
