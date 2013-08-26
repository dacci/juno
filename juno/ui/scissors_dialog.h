// Copyright (c) 2013 dacci.org

#ifndef JUNO_UI_SCISSORS_DIALOG_H_
#define JUNO_UI_SCISSORS_DIALOG_H_

#include <atlbase.h>

#include <atlapp.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include <atlddx.h>
#include <atldlgs.h>

#include "res/resource.h"
#include "ui/preference_dialog.h"

class ScissorsDialog
    : public CDialogImpl<ScissorsDialog>,
      public CWinDataExchange<ScissorsDialog> {
 public:
  static const UINT IDD = IDD_SCISSORS;

  explicit ScissorsDialog(PreferenceDialog::ServiceEntry* entry);
  ~ScissorsDialog();

  BEGIN_DDX_MAP(ScissorsDialog)
    DDX_TEXT(IDC_ADDRESS, address_)
    DDX_CONTROL_HANDLE(IDC_ADDRESS, address_edit_)
    DDX_INT(IDC_PORT, port_)
    DDX_CONTROL_HANDLE(IDC_PORT_SPIN, port_spin_)
    DDX_CONTROL_HANDLE(IDC_USE_SSL, use_ssl_check_)
  END_DDX_MAP()

  BEGIN_MSG_MAP_EX(ScissorsDialog)
    MSG_WM_INITDIALOG(OnInitDialog)

    COMMAND_ID_HANDLER_EX(IDOK, OnOk)
    COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
  END_MSG_MAP()

 private:
  BOOL OnInitDialog(CWindow focus, LPARAM init_param);

  void OnOk(UINT notify_code, int id, CWindow control);
  void OnCancel(UINT notify_code, int id, CWindow control);

  PreferenceDialog::ScissorsEntry* config_;

  CString address_;
  CEdit address_edit_;
  int port_;
  CUpDownCtrl port_spin_;
  CButton use_ssl_check_;
};

#endif  // JUNO_UI_SCISSORS_DIALOG_H_
