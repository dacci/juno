// Copyright (c) 2013 dacci.org

#include "ui/scissors_dialog.h"

ScissorsDialog::ScissorsDialog(PreferenceDialog::ServiceEntry* entry)
    : config_(static_cast<PreferenceDialog::ScissorsEntry*>(entry->extra)) {
}

ScissorsDialog::~ScissorsDialog() {
}

BOOL ScissorsDialog::OnInitDialog(CWindow focus, LPARAM init_param) {
  port_ = config_->remote_port_;

  DoDataExchange();

  address_edit_.SetWindowText(config_->remote_address_);
  port_spin_.SetRange32(0, 65535);
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
  EDITBALLOONTIP balloon = { sizeof(balloon) };
  CString message;

  DoDataExchange(DDX_SAVE);

  if (address_edit_.GetWindowTextLength() == 0) {
    message.LoadString(IDS_NOT_SPECIFIED);
    balloon.pszText = message;
    address_edit_.ShowBalloonTip(&balloon);
    return;
  }

  if (port_ <= 0 || 65536 <= port_) {
    message.LoadString(IDS_INVALID_PORT);
    balloon.pszText = message;
    port_edit_.ShowBalloonTip(&balloon);
    return;
  }

  address_edit_.GetWindowText(config_->remote_address_);
  config_->remote_port_ = port_;
  config_->remote_ssl_ = use_ssl_check_.GetCheck();
  config_->remote_udp_ = use_udp_check_.GetCheck();

  EndDialog(IDOK);
}

void ScissorsDialog::OnCancel(UINT notify_code, int id, CWindow control) {
  EndDialog(IDCANCEL);
}
