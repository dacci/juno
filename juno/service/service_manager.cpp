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

const wchar_t kConfigKeyName[] = L"Software\\dacci.org\\Juno";
const wchar_t kServicesKeyName[] = L"Services";
const wchar_t kServersKeyName[] = L"Servers";

const wchar_t kNameReg[] = L"Name";
const wchar_t kProviderReg[] = L"Provider";
const wchar_t kBindReg[] = L"Bind";
const wchar_t kListenReg[] = L"Listen";
const wchar_t kTypeReg[] = L"Type";
const wchar_t kServiceReg[] = L"Service";
const wchar_t kEnabledReg[] = L"Enabled";
const wchar_t kCertificateReg[] = L"Certificate";

const std::string kIdJson = "id";
const std::string kNameJson = "name";
const std::string kProviderJson = "provider";
const std::string kBindJson = "bind";
const std::string kListenJson = "listen";
const std::string kTypeJson = "type";
const std::string kServiceJson = "service";
const std::string kEnabledJson = "enabled";
const std::string kCertificateJson = "certificate";

class SecureChannelCustomizer : public TcpServer::ChannelCustomizer {
 public:
  std::unique_ptr<io::Channel> Customize(
      std::unique_ptr<io::Channel>&& channel) override {
    return std::make_unique<io::SecureChannel>(&credential_, std::move(channel),
                                               true);
  }

  misc::schannel::SchannelCredential credential_;
};

misc::CertificateStore certificate_store(L"MY");
std::vector<std::unique_ptr<SecureChannelCustomizer>> channel_customizers;

}  // namespace

using ::base::win::RegKey;
using ::base::win::RegistryKeyIterator;

ServiceManager* ServiceManager::instance_ = nullptr;

ServiceManager::ServiceManager() : root_key_(NULL) {
  DCHECK(instance_ == nullptr);
  instance_ = this;

  root_key_ = app::GetApplication()->IsService() ? HKEY_LOCAL_MACHINE
                                                 : HKEY_CURRENT_USER;

#define PROVIDER_ENTRY(key, ns) \
  providers_.insert({L#key, std::make_unique<ns::key##Provider>()})

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

void ServiceManager::LoadServices() {
  RegKey app_key(root_key_, kConfigKeyName, KEY_READ);
  if (!app_key.Valid())
    return;

  RegKey services_key(app_key.Handle(), kServicesKeyName, KEY_READ);
  if (!services_key.Valid())
    return;

  for (RegistryKeyIterator i(services_key.Handle(), nullptr); i.Valid(); ++i)
    LoadService(services_key, i.Name());
}

void ServiceManager::StopServices() {
  StopServers();

  for (auto& pair : services_)
    pair.second->Stop();
  services_.clear();
}

bool ServiceManager::LoadServers() {
  RegKey app_key(root_key_, kConfigKeyName, KEY_READ);
  if (!app_key.Valid())
    return true;

  RegKey servers_key(app_key.Handle(), kServersKeyName, KEY_READ);
  if (!servers_key.Valid())
    return true;

  auto all_succeeded = true;

  for (RegistryKeyIterator i(servers_key.Handle(), nullptr); i.Valid(); ++i) {
    if (!LoadServer(servers_key, i.Name()))
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

ServiceProvider* ServiceManager::GetProvider(const std::wstring& name) const {
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
  SHDeleteKey(root_key_, kConfigKeyName);

  RegKey config_key(root_key_, kConfigKeyName, KEY_ALL_ACCESS);
  if (!config_key.Valid())
    return false;

  RegKey services_key(config_key.Handle(), kServicesKeyName, KEY_ALL_ACCESS);
  if (!services_key.Valid())
    return false;

  RegKey servers_key(config_key.Handle(), kServersKeyName, KEY_ALL_ACCESS);
  if (!servers_key.Valid())
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

  std::wstring provider_name;
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

bool ServiceManager::LoadService(const RegKey& parent, const wchar_t* id) {
  RegKey reg_key(parent.Handle(), id, KEY_READ);
  if (!reg_key.Valid())
    return false;

  std::wstring provider_name;
  if (reg_key.ReadValue(kProviderReg, &provider_name) != ERROR_SUCCESS)
    return false;

  auto provider = GetProvider(provider_name);
  if (provider == nullptr)
    return false;

  auto config = provider->LoadConfig(reg_key);
  if (config == nullptr)
    return false;

  // convert name to GUID
  if (reg_key.HasValue(kNameReg)) {
    config->id_ = id;
    reg_key.ReadValue(kNameReg, &config->name_);
  } else {
    config->id_ = misc::GenerateGUID16();
    config->name_ = id;
  }

  config->provider_ = provider_name;

  auto service_id = config->id_;
  service_configs_.insert({service_id, std::move(config)});

  return CreateService(service_id);
}

bool ServiceManager::SaveService(const RegKey& parent,
                                 const ServiceConfig* config) {
  RegKey service_key(parent.Handle(), config->id_.c_str(), KEY_ALL_ACCESS);
  if (!service_key.Valid())
    return false;

  if (service_key.WriteValue(kNameReg, config->name_.c_str()) !=
          ERROR_SUCCESS ||
      service_key.WriteValue(kProviderReg, config->provider_.c_str()) !=
          ERROR_SUCCESS)
    return false;

  return providers_[config->provider_]->SaveConfig(config, &service_key);
}

bool ServiceManager::CreateService(const std::wstring& id) {
  DCHECK(service_configs_.find(id) != service_configs_.end());

  auto& config = service_configs_[id];
  auto service = providers_[config->provider_]->CreateService(config.get());
  if (service == nullptr)
    return false;

  services_.insert({id, std::move(service)});

  return true;
}

bool ServiceManager::LoadServer(const RegKey& parent, const wchar_t* id) {
  RegKey reg_key(parent.Handle(), id, KEY_READ);
  if (!reg_key.Valid())
    return false;

  std::wstring bind, service;
  DWORD listen, type, enabled;
  if (reg_key.ReadValue(kBindReg, &bind) != ERROR_SUCCESS ||
      reg_key.ReadValueDW(kListenReg, &listen) != ERROR_SUCCESS ||
      reg_key.ReadValueDW(kTypeReg, &type) != ERROR_SUCCESS ||
      reg_key.ReadValue(kServiceReg, &service) != ERROR_SUCCESS ||
      reg_key.ReadValueDW(kEnabledReg, &enabled) != ERROR_SUCCESS)
    return false;

  auto config = std::make_unique<ServerConfig>();
  config->bind_ = base::SysWideToNativeMB(bind);
  config->listen_ = listen;
  config->type_ = type;
  config->service_ = std::move(service);
  config->enabled_ = enabled;

  // convert server name to GUID
  if (reg_key.HasValue(L"Name"))
    config->id_ = misc::GenerateGUID16();
  else
    config->id_ = id;

  // convert service name to GUID
  if (service_configs_.find(config->service_) == service_configs_.end()) {
    for (const auto& pair : service_configs_) {
      if (pair.second->name_ == config->service_) {
        config->service_ = pair.second->id_;
        break;
      }
    }
  }

  DWORD length = 20;
  config->cert_hash_.resize(length);
  reg_key.ReadValue(kCertificateReg, &config->cert_hash_[0], &length, nullptr);
  config->cert_hash_.resize(length);

  auto server_id = config->id_;
  server_configs_.insert({server_id, std::move(config)});

  return CreateServer(server_id);
}

bool ServiceManager::CreateServer(const std::wstring& id) {
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

bool ServiceManager::SaveServer(const RegKey& parent,
                                const ServerConfig* config) {
  RegKey key(parent.Handle(), config->id_.c_str(), KEY_ALL_ACCESS);
  if (!key.Valid())
    return false;

  key.WriteValue(kBindReg, base::SysNativeMBToWide(config->bind_).c_str());
  key.WriteValue(kListenReg, config->listen_);
  key.WriteValue(kTypeReg, config->type_);
  key.WriteValue(kServiceReg, config->service_.c_str());
  key.WriteValue(kEnabledReg, config->enabled_);

  if (!config->cert_hash_.empty())
    key.WriteValue(kCertificateReg, config->cert_hash_.data(),
                   static_cast<DWORD>(config->cert_hash_.size()), REG_BINARY);

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
