// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_HTTP_UI_HTTP_HEADER_FILTER_DIALOG_H_
#define JUNO_SERVICE_HTTP_UI_HTTP_HEADER_FILTER_DIALOG_H_

#include <atlbase.h>
#include <atlwin.h>

#include <atlapp.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include <atldlgs.h>

#include <string>

#include "res/resource.h"
#include "ui/data_exchange_ex.h"

#include "service/http/http_proxy_config.h"

namespace juno {
namespace service {
namespace http {
namespace ui {

class HttpHeaderFilterDialog
    : public CDialogImpl<HttpHeaderFilterDialog>,
      public juno::ui::DataExchangeEx<HttpHeaderFilterDialog> {
 public:
  static const UINT IDD = IDD_HTTP_HEADER_FILTER;

  explicit HttpHeaderFilterDialog(HttpProxyConfig::HeaderFilter* filter);

  BEGIN_MSG_MAP(HttpHeaderFilterDialog)
    MSG_WM_INITDIALOG(OnInitDialog)

    COMMAND_ID_HANDLER_EX(IDOK, OnOk)
    COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
  END_MSG_MAP()

  BEGIN_DDX_MAP(HttpHeaderFilterDialog)
    DDX_CONTROL_HANDLE(IDC_REQUEST, request_check_)
    DDX_CONTROL_HANDLE(IDC_RESPONSE, response_check_)
    DDX_CONTROL_HANDLE(IDC_ACTION, action_combo_)
    DDX_TEXT(IDC_NAME, name_)
    DDX_CONTROL_HANDLE(IDC_NAME, name_edit_)
    DDX_TEXT(IDC_VALUE, value_)
    DDX_CONTROL_HANDLE(IDC_VALUE, value_edit_)
    DDX_TEXT(IDC_REPLACE, replace_)
    DDX_CONTROL_HANDLE(IDC_REPLACE, replace_edit_)
  END_DDX_MAP()

 private:
  BOOL OnInitDialog(CWindow focus, LPARAM init_param);

  void OnOk(UINT notify_code, int id, CWindow control);
  void OnCancel(UINT notify_code, int id, CWindow control);

  HttpProxyConfig::HeaderFilter* filter_;

  CButton request_check_;
  CButton response_check_;
  CComboBox action_combo_;
  std::wstring name_;
  CEdit name_edit_;
  std::wstring value_;
  CEdit value_edit_;
  std::wstring replace_;
  CEdit replace_edit_;

  HttpHeaderFilterDialog(const HttpHeaderFilterDialog&) = delete;
  HttpHeaderFilterDialog& operator=(const HttpHeaderFilterDialog&) = delete;
};

}  // namespace ui
}  // namespace http
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_HTTP_UI_HTTP_HEADER_FILTER_DIALOG_H_
