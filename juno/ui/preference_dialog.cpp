// Copyright (c) 2013 dacci.org

#include "ui/preference_dialog.h"

#include <wincrypt.h>

#include <utility>

#include "res/resource.h"
#include "ui/servers_page.h"
#include "ui/services_page.h"

PreferenceDialog::PreferenceDialog()
    : CPropertySheetImpl(ID_FILE_NEW), centered_() {
  m_psh.dwFlags |= PSH_NOAPPLYNOW | PSH_NOCONTEXTHELP;

  auto service_manager = ServiceManager::GetInstance();
  service_manager->CopyServiceConfigs(&service_configs_);
  service_manager->CopyServerConfigs(&server_configs_);

  AddPage(*new ServicesPage(this, &service_configs_));
  AddPage(*new ServersPage(this, &server_configs_));
}

PreferenceDialog::~PreferenceDialog() {
}

void PreferenceDialog::OnShowWindow(BOOL show, UINT status) {
  SetMsgHandled(FALSE);

  if (show && !centered_) {
    centered_ = true;
    CenterWindow();
  }
}
