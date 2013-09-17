// Copyright (c) 2013 dacci.org

#include "ui/http_header_filter_dialog.h"

#include <boost/regex.hpp>

HttpHeaderFilterDialog::HttpHeaderFilterDialog(
    PreferenceDialog::HttpHeaderFilter* filter) : filter_(filter) {
}

HttpHeaderFilterDialog::~HttpHeaderFilterDialog() {
}

BOOL HttpHeaderFilterDialog::OnInitDialog(CWindow focus, LPARAM init_param) {
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
  action_combo_.SetCurSel(filter_->action);
  name_edit_.SetWindowText(filter_->name);
  value_edit_.SetWindowText(filter_->value);
  replace_edit_.SetWindowText(filter_->replace);

  return TRUE;
}

void HttpHeaderFilterDialog::OnOk(UINT notify_code, int id, CWindow control) {
  CString name, value, replace;
  name_edit_.GetWindowText(name);
  value_edit_.GetWindowText(value);
  replace_edit_.GetWindowText(replace);

  CStringW message;
  message.LoadString(IDS_NOT_SPECIFIED);

  EDITBALLOONTIP balloon = { sizeof(balloon) };
  balloon.pszText = message;

  if (name.IsEmpty()) {
    name_edit_.ShowBalloonTip(&balloon);
    return;
  }

  if (value.IsEmpty()) {
    value_edit_.ShowBalloonTip(&balloon);
    return;
  }

  int action = action_combo_.GetCurSel();
  if (action >= 5) {
    try {
#ifdef UNICODE
      boost::wregex(value.GetString());
#else   // UNICODE
      boost::regex(value.GetString());
#endif  // UNICODE
    } catch (const boost::regex_error& e) {  // NOLINT(*)
      message = e.what();
      balloon.pszText = message;
      value_edit_.ShowBalloonTip(&balloon);
      return;
    }
  }

  filter_->request = request_check_.GetCheck();
  filter_->response = response_check_.GetCheck();
  filter_->action = action;
  filter_->name = name;
  filter_->value = value;
  filter_->replace = replace;

  EndDialog(IDOK);
}

void HttpHeaderFilterDialog::OnCancel(UINT notify_code, int id,
                                      CWindow control) {
  EndDialog(IDCANCEL);
}
