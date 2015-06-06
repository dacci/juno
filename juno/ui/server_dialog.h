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

class ServerConfig;

class ServerDialog
    : public CDialogImpl<ServerDialog>,
      public CWinDataExchange<ServerDialog> {
 public:
  static const UINT IDD = IDD_SERVER;

  ServerDialog(PreferenceDialog* parent, ServerConfig* entry);
  ~ServerDialog();

  BEGIN_DDX_MAP(ServerDialog)
    DDX_TEXT(IDC_BIND, bind_)
    DDX_CONTROL_HANDLE(IDC_BIND, bind_combo_)
    DDX_INT(IDC_LISTEN, listen_)
    DDX_CONTROL_HANDLE(IDC_LISTEN, listen_edit_)
    DDX_CONTROL_HANDLE(IDC_LISTEN_SPIN, listen_spin_)
    DDX_CONTROL_HANDLE(IDC_PROTOCOL, type_combo_)
    DDX_CONTROL_HANDLE(ID_FILE_PAGE_SETUP, detail_button_)
    DDX_CONTROL_HANDLE(IDC_SERVICE, service_combo_)
    DDX_CHECK(IDC_ENABLE, enabled_)
  END_DDX_MAP()

  BEGIN_MSG_MAP(ServerDialog)
    MSG_WM_INITDIALOG(OnInitDialog)

    COMMAND_HANDLER_EX(IDC_PROTOCOL, CBN_SELCHANGE, OnTypeChange)
    COMMAND_ID_HANDLER_EX(ID_FILE_PAGE_SETUP, OnDetailSetting)
    COMMAND_ID_HANDLER_EX(IDOK, OnOk)
    COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
  END_MSG_MAP()

 private:
  void FillBindCombo();
  void FillServiceCombo();

  BOOL OnInitDialog(CWindow focus, LPARAM init_param);

  void OnTypeChange(UINT notify_code, int id, CWindow control);
  void OnDetailSetting(UINT notify_code, int id, CWindow control);
  void OnOk(UINT notify_code, int id, CWindow control);
  void OnCancel(UINT notify_code, int id, CWindow control);

  PreferenceDialog* const parent_;
  ServerConfig* const entry_;

  CString bind_;
  CComboBox bind_combo_;
  int listen_;
  CEdit listen_edit_;
  CUpDownCtrl listen_spin_;
  CComboBox type_combo_;
  CButton detail_button_;
  CComboBox service_combo_;
  BOOL enabled_;
};

#endif  // JUNO_UI_SERVER_DIALOG_H_
