// Copyright (c) 2013 dacci.org

#include "ui/scissors_dialog.h"

#include "net/scissors/scissors_config.h"

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
  CString message;

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

  config_->set_remote_address(temp);
  config_->set_remote_port(port_);
  config_->set_remote_ssl(use_ssl_check_.GetCheck());
  config_->set_remote_udp(use_udp_check_.GetCheck());

  EndDialog(IDOK);
}

void ScissorsDialog::OnCancel(UINT notify_code, int id, CWindow control) {
  EndDialog(IDCANCEL);
}
