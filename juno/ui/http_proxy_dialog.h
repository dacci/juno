// Copyright (c) 2013 dacci.org

#ifndef JUNO_UI_HTTP_PROXY_DIALOG_H_
#define JUNO_UI_HTTP_PROXY_DIALOG_H_

#include <atlbase.h>

#include <atlapp.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include <atlddx.h>
#include <atldlgs.h>

#include "res/resource.h"

class HttpProxyDialog
    : public CDialogImpl<HttpProxyDialog>,
      public CWinDataExchange<HttpProxyDialog> {
 public:
  static const UINT IDD = IDD_HTTP_PROXY;

  HttpProxyDialog();
  ~HttpProxyDialog();

  BEGIN_DDX_MAP(HttpProxyDialog)
    DDX_CONTROL_HANDLE(IDC_USE_REMOTE_PROXY, use_remote_proxy_check_)
    DDX_CONTROL_HANDLE(IDC_ADDRESS, address_edit_)
    DDX_INT(IDC_PORT, port_)
    DDX_CONTROL_HANDLE(IDC_PORT_SPIN, port_spin_)
    DDX_CONTROL_HANDLE(IDC_AUTH_REMOTE, auth_remote_check_)
    DDX_CONTROL_HANDLE(IDC_REMOTE_USER, remote_user_edit_)
    DDX_TEXT(IDC_REMOTE_PASSWORD, remote_password_)
  END_DDX_MAP()

  BEGIN_MSG_MAP(HttpProxyDialog)
    MSG_WM_INITDIALOG(OnInitDialog)

    COMMAND_ID_HANDLER_EX(IDOK, OnOk)
    COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
  END_MSG_MAP()

 private:
  BOOL OnInitDialog(CWindow focus, LPARAM init_param);

  void OnOk(UINT notify_code, int id, CWindow control);
  void OnCancel(UINT notify_code, int id, CWindow control);

  CButton use_remote_proxy_check_;
  CEdit address_edit_;
  int port_;
  CUpDownCtrl port_spin_;
  CButton auth_remote_check_;
  CEdit remote_user_edit_;
  CString remote_password_;
};

#endif  // JUNO_UI_HTTP_PROXY_DIALOG_H_
