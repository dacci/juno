// Copyright (c) 2013 dacci.org

#include "service/http/ui/http_header_filter_dialog.h"

#include <atlstr.h>

#include <base/strings/sys_string_conversions.h>

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
  name_ = base::SysUTF8ToWide(filter_->name);
  value_ = base::SysUTF8ToWide(filter_->value);
  replace_ = base::SysUTF8ToWide(filter_->replace);

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

  return TRUE;
}

void HttpHeaderFilterDialog::OnOk(UINT /*notify_code*/, int /*id*/,
                                  CWindow /*control*/) {
  CString message;
  message.LoadString(IDS_NOT_SPECIFIED);

  EDITBALLOONTIP balloon{sizeof(balloon)};
  balloon.pszText = message;

  DoDataExchange(DDX_SAVE);

  if (name_.empty()) {
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
      std::wregex regex(value_);
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
  filter_->name = base::SysWideToUTF8(name_);
  filter_->value = base::SysWideToUTF8(value_);
  filter_->replace = base::SysWideToUTF8(replace_);

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
