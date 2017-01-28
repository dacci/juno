// Copyright (c) 2017 dacci.org

#include "app/service_configurator.h"

#include <base/logging.h>

#include "app/application.h"
#include "app/constants.h"
#include "res/resource.h"
#include "ui/preference_dialog.h"

namespace juno {
namespace app {

ServiceConfigurator::ServiceConfigurator()
    : event_(CreateEvent(nullptr, FALSE, FALSE, nullptr)),
      loaded_(false),
      saved_(false) {
  if (event_ == NULL) {
    LOG(ERROR) << "Failed to create event: " << GetLastError();
    return;
  }

  manager_ = std::make_unique<service::ServiceManager>();
  if (manager_ == nullptr) {
    LOG(ERROR) << "Failed to allocate ServiceManager.";
    return;
  }
}

ServiceConfigurator::~ServiceConfigurator() {
  if (event_ != NULL) {
    CloseHandle(event_);
    event_ = NULL;
  }
}

void ServiceConfigurator::Configure() {
  auto application = app::GetApplication();

  if (event_ == NULL || manager_ == nullptr) {
    application->ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_INIT_FAILED);
    return;
  }

  service::rpc::RpcClient client;
  auto result = client.Connect(kRpcServiceName);
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to connect: 0x" << std::hex << result;
    application->ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_INIT_FAILED);
    return;
  }

  result = client.CallMethod(kConfigGetMethod, nullptr, this);
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to get config: 0x" << std::hex << result;
    application->ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_INIT_FAILED);
    return;
  }

  auto wait = WaitForSingleObject(event_, kTimeout);
  if (wait != WAIT_OBJECT_0) {
    LOG(ERROR) << "Failed to wait method result: " << GetLastError();
    application->ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_INIT_FAILED);
    return;
  }

  if (!loaded_) {
    LOG(ERROR) << "Failed to get config.";
    application->ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_INIT_FAILED);
    return;
  }

  ui::PreferenceDialog dialog;
  if (dialog.DoModal(NULL) != IDOK)
    return;

  auto services = manager_->ConvertServiceConfig(&dialog.service_configs_);
  auto servers =
      service::ServiceManager::ConvertServerConfig(&dialog.server_configs_);
  if (services == nullptr || servers == nullptr) {
    LOG(ERROR) << "Failed to convert config.";
    application->ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_UNEXPECTED);
    return;
  }

  base::DictionaryValue params;
  params.Set("services", std::move(services));
  params.Set("servers", std::move(servers));

  result = client.CallMethod(kConfigSetMethod, &params, this);
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to set config: 0x" << std::hex << result;
    application->ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_UNEXPECTED);
    return;
  }

  wait = WaitForSingleObject(event_, kTimeout);
  if (wait != WAIT_OBJECT_0) {
    LOG(ERROR) << "Failed to wait method result: " << GetLastError();
    application->ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_UNEXPECTED);
    return;
  }

  if (!saved_) {
    LOG(ERROR) << "Failed to save config.";
    application->ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_UNEXPECTED);
  }
}

void ServiceConfigurator::OnReturned(const char* method,
                                     const base::Value* result,
                                     const base::DictionaryValue* /*error*/) {
  if (strcmp(method, kConfigGetMethod) == 0) {
    if (result != nullptr && result->IsType(base::Value::TYPE_DICTIONARY)) {
      const base::DictionaryValue* object = nullptr;
      result->GetAsDictionary(&object);

      const base::ListValue* services = nullptr;
      const base::ListValue* servers = nullptr;
      if (object->GetList("services", &services) &&
          object->GetList("servers", &servers)) {
        loaded_ =
            manager_->ConvertServiceConfig(services,
                                           &manager_->service_configs_) &&
            manager_->ConvertServerConfig(servers, &manager_->server_configs_);
      }
    }
  } else if (strcmp(method, kConfigSetMethod) == 0) {
    if (result != nullptr && result->IsType(base::Value::TYPE_BOOLEAN))
      result->GetAsBoolean(&saved_);
  }

  SetEvent(event_);
}

}  // namespace app
}  // namespace juno
