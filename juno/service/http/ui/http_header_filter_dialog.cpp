// Copyright (c) 2013 dacci.org

#include "service/http/ui/http_header_filter_dialog.h"

#include <regex>
#include <string>

namespace juno {
namespace service {
namespace http {
namespace ui {

HttpHeaderFilterDialog::HttpHeaderFilterDialog(
    HttpProxyConfig::HeaderFilter* filter)
    : filter_(filter) {}

BOOL HttpHeaderFilterDialog::OnInitDialog(CWindow /*focus*/,
                                          LPARAM /*init_param*/) {
  DoDataExchange();

  action_combo_.AddString(_T("Set"));
  action_combo_.AddString(_T("Append"));
  action_combo_.AddString(_T("Add"));
  action_combo_.AddString(_T("Unset"));
  action_combo_.AddString(_T("Merge"));
  action_combo_.AddString(_T("Edit"));
  action_combo_.AddString(_T("Edit*"));

  request_check_.SetCheck(filter_->request);
  response_check_.SetCheck(filter_->response);
  action_combo_.SetCurSel(static_cast<int>(filter_->action));
  name_edit_.SetWindowText(CString(filter_->name.c_str()));
  value_edit_.SetWindowText(CString(filter_->value.c_str()));
  replace_edit_.SetWindowText(CString(filter_->replace.c_str()));

  return TRUE;
}

void HttpHeaderFilterDialog::OnOk(UINT /*notify_code*/, int /*id*/,
                                  CWindow /*control*/) {
  CStringW message;
  message.LoadString(IDS_NOT_SPECIFIED);

  EDITBALLOONTIP balloon{sizeof(balloon)};
  balloon.pszText = message;

  if (name_edit_.GetWindowTextLength() <= 0) {
    name_edit_.ShowBalloonTip(&balloon);
    return;
  }

  auto action = action_combo_.GetCurSel();
  if (action != 3 && value_edit_.GetWindowTextLength() <= 0) {
    value_edit_.ShowBalloonTip(&balloon);
    return;
  }

  if (action >= 5) {
    try {
      CString value;
      value_edit_.GetWindowText(value);
#ifdef UNICODE
      std::wregex(value.GetString());
#else   // UNICODE
      std::regex(value.GetString());
#endif  // UNICODE
    } catch (const std::regex_error& e) {
      message = e.what();
      balloon.pszText = message;
      value_edit_.ShowBalloonTip(&balloon);
      return;
    }
  }

  filter_->request = request_check_.GetCheck() != FALSE;
  filter_->response = response_check_.GetCheck() != FALSE;
  filter_->action = static_cast<HttpProxyConfig::FilterAction>(action);

  std::string temp;

  temp.resize(GetWindowTextLengthA(name_edit_));
  GetWindowTextA(name_edit_, &temp[0], static_cast<int>(temp.size() + 1));
  filter_->name = temp;

  temp.resize(GetWindowTextLengthA(value_edit_));
  GetWindowTextA(value_edit_, &temp[0], static_cast<int>(temp.size() + 1));
  filter_->value = temp;

  temp.resize(GetWindowTextLengthA(replace_edit_));
  GetWindowTextA(replace_edit_, &temp[0], static_cast<int>(temp.size() + 1));
  filter_->replace = temp;

  EndDialog(IDOK);
}

void HttpHeaderFilterDialog::OnCancel(UINT /*notify_code*/, int /*id*/,
                                      CWindow /*control*/) {
  EndDialog(IDCANCEL);
}

}  // namespace ui
}  // namespace http
}  // namespace service
}  // namespace juno
