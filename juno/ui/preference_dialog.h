// Copyright (c) 2013 dacci.org

#ifndef JUNO_UI_PREFERENCE_DIALOG_H_
#define JUNO_UI_PREFERENCE_DIALOG_H_

#include <atlbase.h>

#include <atlapp.h>
#include <atlcrack.h>
#include <atldlgs.h>

#include <map>
#include <utility>

class ServerConfig;
class ServiceConfig;

class PreferenceDialog : public CPropertySheetImpl<PreferenceDialog> {
 public:
  PreferenceDialog();
  ~PreferenceDialog();

  BEGIN_MSG_MAP(PreferenceDialog)
    MSG_WM_SHOWWINDOW(OnShowWindow)

    CHAIN_MSG_MAP(CPropertySheetImpl)
  END_MSG_MAP()

  std::map<std::string, ServiceConfig*> service_configs_;
  std::map<std::string, ServerConfig*> server_configs_;

 private:
  void OnShowWindow(BOOL show, UINT status);

  bool centered_;
};

#endif  // JUNO_UI_PREFERENCE_DIALOG_H_
