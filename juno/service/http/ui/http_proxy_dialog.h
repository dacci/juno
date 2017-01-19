// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_HTTP_UI_HTTP_PROXY_DIALOG_H_
#define JUNO_SERVICE_HTTP_UI_HTTP_PROXY_DIALOG_H_

#include <atlbase.h>
#include <atlwin.h>

#include <atlapp.h>
#include <atlcrack.h>
#include <atlctrls.h>

#include <string>
#include <vector>

#include "res/resource.h"
#include "ui/data_exchange_ex.h"

#include "service/http/http_proxy_config.h"

namespace juno {
namespace service {
namespace http {
namespace ui {

class HttpProxyDialog : public CDialogImpl<HttpProxyDialog>,
                        public juno::ui::DataExchangeEx<HttpProxyDialog> {
 public:
  static const UINT IDD = IDD_HTTP_PROXY;

  explicit HttpProxyDialog(HttpProxyConfig* config);

  BEGIN_MSG_MAP(HttpProxyDialog)
    MSG_WM_INITDIALOG(OnInitDialog)

    COMMAND_ID_HANDLER_EX(IDC_ADD_BUTTON, OnAddFilter)
    COMMAND_ID_HANDLER_EX(IDC_EDIT_BUTTON, OnEditFilter)
    COMMAND_ID_HANDLER_EX(IDC_DELETE_BUTTON, OnDeleteFilter)
    COMMAND_ID_HANDLER_EX(ID_SCROLL_UP, OnScrollUp)
    COMMAND_ID_HANDLER_EX(ID_SCROLL_DOWN, OnScrollDown)
    COMMAND_ID_HANDLER_EX(IDOK, OnOk)
    COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)

    NOTIFY_HANDLER_EX(IDC_FILTER_LIST, NM_DBLCLK, OnFilterListDoubleClicked)
  END_MSG_MAP()

  BEGIN_DDX_MAP(HttpProxyDialog)
    DDX_CONTROL_HANDLE(IDC_USE_REMOTE_PROXY, use_remote_proxy_check_)
    DDX_TEXT(IDC_ADDRESS, address_)
    DDX_CONTROL_HANDLE(IDC_ADDRESS, address_edit_)
    DDX_INT(IDC_PORT, port_)
    DDX_CONTROL_HANDLE(IDC_PORT, port_edit_)
    DDX_CONTROL_HANDLE(IDC_PORT_SPIN, port_spin_)
    DDX_CONTROL_HANDLE(IDC_AUTH_REMOTE, auth_remote_check_)
    DDX_TEXT(IDC_REMOTE_USER, remote_user_)
    DDX_CONTROL_HANDLE(IDC_REMOTE_USER, remote_user_edit_)
    DDX_TEXT(IDC_REMOTE_PASSWORD, remote_password_)
    DDX_CONTROL_HANDLE(IDC_REMOTE_PASSWORD, remote_password_edit_)
    DDX_CONTROL_HANDLE(IDC_FILTER_LIST, filter_list_)
    DDX_CONTROL_HANDLE(IDC_ADD_BUTTON, add_button_)
    DDX_CONTROL_HANDLE(IDC_EDIT_BUTTON, edit_button_)
    DDX_CONTROL_HANDLE(IDC_DELETE_BUTTON, delete_button_)
    DDX_CONTROL_HANDLE(ID_SCROLL_UP, up_button_)
    DDX_CONTROL_HANDLE(ID_SCROLL_DOWN, down_button_)
  END_DDX_MAP()

 private:
  void AddFilterItem(const HttpProxyConfig::HeaderFilter& filter,
                     int filter_index, int index);

  BOOL OnInitDialog(CWindow focus, LPARAM init_param);

  void OnAddFilter(UINT notify_code, int id, CWindow control);
  void OnEditFilter(UINT notify_code, int id, CWindow control);
  void OnDeleteFilter(UINT notify_code, int id, CWindow control);
  void OnScrollUp(UINT notify_code, int id, CWindow control);
  void OnScrollDown(UINT notify_code, int id, CWindow control);
  void OnOk(UINT notify_code, int id, CWindow control);
  void OnCancel(UINT notify_code, int id, CWindow control);

  LRESULT OnFilterListDoubleClicked(LPNMHDR header);

  HttpProxyConfig* const config_;
  std::vector<HttpProxyConfig::HeaderFilter> filters_;

  CButton use_remote_proxy_check_;
  std::wstring address_;
  CEdit address_edit_;
  int port_;
  CEdit port_edit_;
  CUpDownCtrl port_spin_;
  CButton auth_remote_check_;
  std::wstring remote_user_;
  CEdit remote_user_edit_;
  std::wstring remote_password_;
  CEdit remote_password_edit_;
  CListViewCtrl filter_list_;
  CImageList image_list_;
  CButton add_button_;
  CButton edit_button_;
  CButton delete_button_;
  CButton up_button_;
  CButton down_button_;

  HttpProxyDialog(const HttpProxyDialog&) = delete;
  HttpProxyDialog& operator=(const HttpProxyDialog&) = delete;
};

}  // namespace ui
}  // namespace http
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_HTTP_UI_HTTP_PROXY_DIALOG_H_
