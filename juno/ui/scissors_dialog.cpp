// Copyright (c) 2013 dacci.org

#include "ui/scissors_dialog.h"

#include <string>

#include "service/scissors/scissors_config.h"

ScissorsDialog::ScissorsDialog(ServiceConfig* config)
    : config_(static_cast<ScissorsConfig*>(config)) {
}

ScissorsDialog::~ScissorsDialog() {
}

BOOL ScissorsDialog::OnInitDialog(CWindow focus, LPARAM init_param) {
  port_ = config_->remote_port();

  DoDataExchange();

  address_edit_.SetWindowText(CString(config_->remote_address().c_str()));
  port_spin_.SetRange32(0, 65535);
  use_ssl_check_.SetCheck(config_->remote_ssl());
  use_udp_check_.SetCheck(config_->remote_udp());

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
  CStringW message;

  DoDataExchange(DDX_SAVE);

  if (address_edit_.GetWindowTextLength() <= 0) {
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

  std::string temp;
  temp.resize(::GetWindowTextLengthA(address_edit_));
  ::GetWindowTextA(address_edit_, &temp[0], temp.size() + 1);

  config_->remote_address_ = temp;
  config_->remote_port_ = port_;
  config_->remote_ssl_ = use_ssl_check_.GetCheck() != FALSE;
  config_->remote_udp_ = use_udp_check_.GetCheck() != FALSE;

  EndDialog(IDOK);
}

void ScissorsDialog::OnCancel(UINT notify_code, int id, CWindow control) {
  EndDialog(IDCANCEL);
}
