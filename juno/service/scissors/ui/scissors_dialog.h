// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_UI_SCISSORS_DIALOG_H_
#define JUNO_SERVICE_SCISSORS_UI_SCISSORS_DIALOG_H_

#include <atlbase.h>
#include <atlwin.h>

#include <atlapp.h>
#include <atlcrack.h>
#include <atlctrls.h>

#include <string>

#include "res/resource.h"
#include "ui/data_exchange_ex.h"
#include "service/scissors/scissors_config.h"

namespace juno {
namespace service {
namespace scissors {
namespace ui {

class ScissorsDialog : public CDialogImpl<ScissorsDialog>,
                       public juno::ui::DataExchangeEx<ScissorsDialog> {
 public:
  static const UINT IDD = IDD_SCISSORS;

  explicit ScissorsDialog(ScissorsConfig* config);

  BEGIN_MSG_MAP(ScissorsDialog)
    MSG_WM_INITDIALOG(OnInitDialog)

    COMMAND_ID_HANDLER_EX(IDC_USE_SSL, OnUseSsl)
    COMMAND_ID_HANDLER_EX(IDC_USE_UDP, OnUseUdp)
    COMMAND_ID_HANDLER_EX(IDOK, OnOk)
    COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
  END_MSG_MAP()

  BEGIN_DDX_MAP(ScissorsDialog)
    DDX_TEXT(IDC_ADDRESS, address_)
    DDX_CONTROL_HANDLE(IDC_ADDRESS, address_edit_)
    DDX_INT(IDC_PORT, port_)
    DDX_CONTROL_HANDLE(IDC_PORT, port_edit_)
    DDX_CONTROL_HANDLE(IDC_PORT_SPIN, port_spin_)
    DDX_CONTROL_HANDLE(IDC_USE_SSL, use_ssl_check_)
    DDX_CONTROL_HANDLE(IDC_USE_UDP, use_udp_check_)
  END_DDX_MAP()

 private:
  BOOL OnInitDialog(CWindow focus, LPARAM init_param);

  void OnUseSsl(UINT notify_code, int id, CWindow control);
  void OnUseUdp(UINT notify_code, int id, CWindow control);
  void OnOk(UINT notify_code, int id, CWindow control);
  void OnCancel(UINT notify_code, int id, CWindow control);

  ScissorsConfig* const config_;

  std::wstring address_;
  CEdit address_edit_;
  int port_;
  CEdit port_edit_;
  CUpDownCtrl port_spin_;
  CButton use_ssl_check_;
  CButton use_udp_check_;

  ScissorsDialog(const ScissorsDialog&) = delete;
  ScissorsDialog& operator=(const ScissorsDialog&) = delete;
};

}  // namespace ui
}  // namespace scissors
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SCISSORS_UI_SCISSORS_DIALOG_H_
