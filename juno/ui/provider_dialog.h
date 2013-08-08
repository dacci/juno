// Copyright (c) 2013 dacci.org

#ifndef JUNO_UI_PROVIDER_DIALOG_H_
#define JUNO_UI_PROVIDER_DIALOG_H_

#include <atlbase.h>

#include <atlapp.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include <atlddx.h>
#include <atldlgs.h>

#include "res/resource.h"

class ProviderDialog
    : public CDialogImpl<ProviderDialog>,
      public CWinDataExchange<ProviderDialog> {
 public:
  static const UINT IDD = IDD_PROVIDER;

  ProviderDialog();
  ~ProviderDialog();

  BEGIN_DDX_MAP(ProviderDialog)
    DDX_CONTROL_HANDLE(IDC_PROVIDER, provider_combo_)
  END_DDX_MAP()

  BEGIN_MSG_MAP(ProviderDialog)
    MSG_WM_INITDIALOG(OnInitDialog)

    COMMAND_ID_HANDLER_EX(IDOK, OnOk)
    COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
  END_MSG_MAP()

 private:
  BOOL OnInitDialog(CWindow focus, LPARAM init_param);

  void OnOk(UINT notify_code, int id, CWindow control);
  void OnCancel(UINT notify_code, int id, CWindow control);

  CComboBox provider_combo_;
};

#endif  // JUNO_UI_PROVIDER_DIALOG_H_
