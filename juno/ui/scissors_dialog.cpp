// Copyright (c) 2013 dacci.org

#include "ui/scissors_dialog.h"

ScissorsDialog::ScissorsDialog(PreferenceDialog::ServiceEntry* entry)
    : config_(static_cast<PreferenceDialog::ScissorsEntry*>(entry->extra)) {
}

ScissorsDialog::~ScissorsDialog() {
}

BOOL ScissorsDialog::OnInitDialog(CWindow focus, LPARAM init_param) {
  address_ = config_->remote_address_;
  port_ = config_->remote_port_;

  DoDataExchange();

  port_spin_.SetRange32(1, 65535);
  use_ssl_check_.SetCheck(config_->remote_ssl_);
  use_udp_check_.SetCheck(config_->remote_udp_);

  return TRUE;
}

void ScissorsDialog::OnUseSsl(UINT notify_code, int id, CWindow control) {
  if (use_ssl_check_.GetCheck())
    use_udp_check_.SetCheck(FALSE);
}

void ScissorsDialog::OnUseUdp(UINT notify_code, int id, CWindow control) {
  if (use_udp_check_.GetCheck())
    use_ssl_check_.SetCheck(FALSE);
}

void ScissorsDialog::OnOk(UINT notify_code, int id, CWindow control) {
  CString message;
  message.LoadString(IDS_NOT_SPECIFIED);

  EDITBALLOONTIP balloon = { sizeof(balloon) };
  balloon.pszText = message;

  DoDataExchange(DDX_SAVE);

  if (address_.IsEmpty()) {
    address_edit_.ShowBalloonTip(&balloon);
    return;
  }

  config_->remote_address_ = address_;
  config_->remote_port_ = port_;
  config_->remote_ssl_ = use_ssl_check_.GetCheck();
  config_->remote_udp_ = use_udp_check_.GetCheck();

  EndDialog(IDOK);
}

void ScissorsDialog::OnCancel(UINT notify_code, int id, CWindow control) {
  EndDialog(IDCANCEL);
}
