// Copyright (c) 2013 dacci.org

#include "ui/preference_dialog.h"

#include <string>
#include <utility>

#include "ui/servers_page.h"
#include "ui/services_page.h"

namespace juno {
namespace ui {

PreferenceDialog::PreferenceDialog()
    : CPropertySheetImpl(ID_FILE_NEW), centered_() {
  m_psh.dwFlags |= PSH_NOAPPLYNOW | PSH_NOCONTEXTHELP;

  auto service_manager = service::ServiceManager::GetInstance();
  service_manager->CopyServiceConfigs(&service_configs_);
  service_manager->CopyServerConfigs(&server_configs_);

  AddPage(*new ServicesPage(this, &service_configs_));
  AddPage(*new ServersPage(this, &server_configs_));
}

CString PreferenceDialog::GetServiceName(const std::string& id) const {
  for (auto& pair : service_configs_) {
    if (pair.second->id_ == id)
      return pair.second->name_.c_str();
  }

  return CString();
}

std::string PreferenceDialog::GetServiceId(const CString& name) const {
  for (auto& pair : service_configs_) {
    if (pair.second->name_.c_str() == name)
      return pair.second->id_;
  }

  return std::string();
}

void PreferenceDialog::OnShowWindow(BOOL show, UINT /*status*/) {
  SetMsgHandled(FALSE);

  if (show && !centered_) {
    centered_ = true;
    CenterWindow();
  }
}

}  // namespace ui
}  // namespace juno
