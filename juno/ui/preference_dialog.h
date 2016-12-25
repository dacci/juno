// Copyright (c) 2013 dacci.org

#ifndef JUNO_UI_PREFERENCE_DIALOG_H_
#define JUNO_UI_PREFERENCE_DIALOG_H_

#include <atlbase.h>
#include <atlwin.h>

#include <atlapp.h>
#include <atlcrack.h>
#include <atldlgs.h>

#include "service/service_manager.h"

class PreferenceDialog : public CPropertySheetImpl<PreferenceDialog> {
 public:
  PreferenceDialog();

  BEGIN_MSG_MAP(PreferenceDialog)
    MSG_WM_SHOWWINDOW(OnShowWindow)

    CHAIN_MSG_MAP(CPropertySheetImpl)
  END_MSG_MAP()

  ServiceConfigMap service_configs_;
  ServerConfigMap server_configs_;

 private:
  void OnShowWindow(BOOL show, UINT status);

  bool centered_;

  PreferenceDialog(const PreferenceDialog&) = delete;
  PreferenceDialog& operator=(const PreferenceDialog&) = delete;
};

#endif  // JUNO_UI_PREFERENCE_DIALOG_H_
