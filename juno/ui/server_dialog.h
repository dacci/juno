// Copyright (c) 2013 dacci.org

#ifndef JUNO_UI_SERVER_DIALOG_H_
#define JUNO_UI_SERVER_DIALOG_H_

#include <atlbase.h>

#include <atlapp.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include <atlddx.h>
#include <atldlgs.h>

#include "res/resource.h"
#include "ui/preference_dialog.h"

class ServerDialog
    : public CDialogImpl<ServerDialog>,
      public CWinDataExchange<ServerDialog> {
 public:
  static const UINT IDD = IDD_SERVER;

  explicit ServerDialog(PreferenceDialog* parent,
                        PreferenceDialog::ServerEntry* entry);
  ~ServerDialog();

  BEGIN_DDX_MAP(ServerDialog)
    DDX_TEXT(IDC_NAME, name_)
    DDX_CONTROL_HANDLE(IDC_BIND, bind_combo_)
    DDX_INT(IDC_LISTEN, listen_)
    DDX_CONTROL_HANDLE(IDC_LISTEN_SPIN, listen_spin_)
    DDX_CONTROL_HANDLE(IDC_SERVICE, service_combo_)
    DDX_CONTROL_HANDLE(IDC_ENABLE, enable_check_)
  END_DDX_MAP()

  BEGIN_MSG_MAP(ServerDialog)
    MSG_WM_INITDIALOG(OnInitDialog)

    COMMAND_ID_HANDLER_EX(IDOK, OnOk)
    COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
  END_MSG_MAP()

 private:
  void FillBindCombo();
  void FillServiceCombo();

  BOOL OnInitDialog(CWindow focus, LPARAM init_param);

  void OnOk(UINT notify_code, int id, CWindow control);
  void OnCancel(UINT notify_code, int id, CWindow control);

  PreferenceDialog* parent_;
  PreferenceDialog::ServerEntry* entry_;

  CString name_;
  CComboBox bind_combo_;
  int listen_;
  CUpDownCtrl listen_spin_;
  CComboBox service_combo_;
  CButton enable_check_;
};

#endif  // JUNO_UI_SERVER_DIALOG_H_
