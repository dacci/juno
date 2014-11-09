// Copyright (c) 2013 dacci.org

#ifndef JUNO_UI_HTTP_HEADER_FILTER_DIALOG_H_
#define JUNO_UI_HTTP_HEADER_FILTER_DIALOG_H_

#include <atlbase.h>

#include <atlapp.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include <atlddx.h>
#include <atldlgs.h>

#include "res/resource.h"
#include "service/http/http_proxy_config.h"
#include "ui/preference_dialog.h"

class HttpHeaderFilterDialog
    : public CDialogImpl<HttpHeaderFilterDialog>,
      public CWinDataExchange<HttpHeaderFilterDialog> {
 public:
  static const UINT IDD = IDD_HTTP_HEADER_FILTER;

  explicit HttpHeaderFilterDialog(HttpProxyConfig::HeaderFilter* filter);
  ~HttpHeaderFilterDialog();

  BEGIN_DDX_MAP(HttpHeaderFilterDialog)
    DDX_CONTROL_HANDLE(IDC_REQUEST, request_check_)
    DDX_CONTROL_HANDLE(IDC_RESPONSE, response_check_)
    DDX_CONTROL_HANDLE(IDC_ACTION, action_combo_)
    DDX_CONTROL_HANDLE(IDC_NAME, name_edit_)
    DDX_CONTROL_HANDLE(IDC_VALUE, value_edit_)
    DDX_CONTROL_HANDLE(IDC_REPLACE, replace_edit_)
  END_DDX_MAP()

  BEGIN_MSG_MAP(HttpHeaderFilterDialog)
    MSG_WM_INITDIALOG(OnInitDialog)

    COMMAND_ID_HANDLER_EX(IDOK, OnOk)
    COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
  END_MSG_MAP()

 private:
  BOOL OnInitDialog(CWindow focus, LPARAM init_param);

  void OnOk(UINT notify_code, int id, CWindow control);
  void OnCancel(UINT notify_code, int id, CWindow control);

  HttpProxyConfig::HeaderFilter* filter_;

  CButton request_check_;
  CButton response_check_;
  CComboBox action_combo_;
  CEdit name_edit_;
  CEdit value_edit_;
  CEdit replace_edit_;
};

#endif  // JUNO_UI_HTTP_HEADER_FILTER_DIALOG_H_
