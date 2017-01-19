// Copyright (c) 2013 dacci.org

#include "ui/preference_dialog.h"

#include <base/strings/string_util.h>

#include <string>
#include <utility>

namespace juno {
namespace ui {

PreferenceDialog::PreferenceDialog()
    : CPropertySheetImpl(ID_FILE_NEW),
      services_page_(this),
      servers_page_(this),
      centered_() {
  m_psh.dwFlags |= PSH_NOAPPLYNOW | PSH_NOCONTEXTHELP;

  auto service_manager = service::ServiceManager::GetInstance();
  service_manager->CopyServiceConfigs(&service_configs_);
  service_manager->CopyServerConfigs(&server_configs_);

  AddPage(services_page_);
  AddPage(servers_page_);
}

const wchar_t* PreferenceDialog::GetServiceName(const std::wstring& id) const {
  auto iter = service_configs_.find(id);
  if (iter == service_configs_.end())
    return nullptr;

  return iter->second->name_.c_str();
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
