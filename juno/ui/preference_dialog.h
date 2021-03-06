// Copyright (c) 2013 dacci.org

#ifndef JUNO_UI_PREFERENCE_DIALOG_H_
#define JUNO_UI_PREFERENCE_DIALOG_H_

#include <atlbase.h>
#include <atlwin.h>

#include <atlapp.h>
#include <atlcrack.h>
#include <atldlgs.h>

#include <string>

#include "service/service_manager.h"
#include "ui/servers_page.h"
#include "ui/services_page.h"

namespace juno {
namespace ui {

class PreferenceDialog : public CPropertySheetImpl<PreferenceDialog> {
 public:
  PreferenceDialog();

  const wchar_t* GetServiceName(const std::wstring& id) const;

  BEGIN_MSG_MAP(PreferenceDialog)
    MSG_WM_SHOWWINDOW(OnShowWindow)

    CHAIN_MSG_MAP(CPropertySheetImpl)
  END_MSG_MAP()

  service::ServiceConfigMap service_configs_;
  service::ServerConfigMap server_configs_;

 private:
  void OnShowWindow(BOOL show, UINT status);

  ServicesPage services_page_;
  ServersPage servers_page_;
  bool centered_;

  PreferenceDialog(const PreferenceDialog&) = delete;
  PreferenceDialog& operator=(const PreferenceDialog&) = delete;
};

}  // namespace ui
}  // namespace juno

#endif  // JUNO_UI_PREFERENCE_DIALOG_H_
