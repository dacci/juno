// Copyright (c) 2013 dacci.org

#ifndef JUNO_UI_SERVER_DIALOG_H_
#define JUNO_UI_SERVER_DIALOG_H_

#include <atlbase.h>
#include <atlwin.h>

#include <atlapp.h>
#include <atlcrack.h>
#include <atlctrls.h>

#include <string>

#include "res/resource.h"
#include "service/server_config.h"
#include "ui/data_exchange_ex.h"
#include "ui/dialog_impl_ex.h"

namespace juno {
namespace ui {

class PreferenceDialog;

class ServerDialog : public DialogImplEx<ServerDialog>,
                     public DataExchangeEx<ServerDialog> {
 public:
  static const UINT IDD = IDD_SERVER;

  ServerDialog(PreferenceDialog* parent, service::ServerConfig* config);

  BEGIN_MSG_MAP(ServerDialog)
    MSG_WM_INITDIALOG(OnInitDialog)

    COMMAND_HANDLER_EX(IDC_PROTOCOL, CBN_SELCHANGE, OnTypeChange)
    COMMAND_ID_HANDLER_EX(ID_FILE_PAGE_SETUP, OnDetailSetting)
    COMMAND_ID_HANDLER_EX(IDOK, OnOk)
    COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)

    CHAIN_MSG_MAP(DialogImplEx)
  END_MSG_MAP()

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

 private:
  void FillBindCombo();
  void FillServiceCombo();

  BOOL OnInitDialog(CWindow focus, LPARAM init_param);

  void OnTypeChange(UINT notify_code, int id, CWindow control);
  void OnDetailSetting(UINT notify_code, int id, CWindow control);
  void OnOk(UINT notify_code, int id, CWindow control);
  void OnCancel(UINT notify_code, int id, CWindow control);

  PreferenceDialog* const parent_;
  service::ServerConfig* const config_;

  std::wstring bind_;
  CComboBox bind_combo_;
  int listen_;
  CEdit listen_edit_;
  CUpDownCtrl listen_spin_;
  CComboBox type_combo_;
  CButton detail_button_;
  CComboBox service_combo_;
  BOOL enabled_;

  ServerDialog(const ServerDialog&) = delete;
  ServerDialog& operator=(const ServerDialog&) = delete;
};

}  // namespace ui
}  // namespace juno

#endif  // JUNO_UI_SERVER_DIALOG_H_
