// Copyright (c) 2013 dacci.org

#include "service/scissors/ui/scissors_dialog.h"

#include <base/strings/sys_string_conversions.h>

#include <string>

#include "service/scissors/scissors_config.h"

namespace juno {
namespace service {
namespace scissors {
namespace ui {

ScissorsDialog::ScissorsDialog(ScissorsConfig* config)
    : config_(config), port_(0) {}

BOOL ScissorsDialog::OnInitDialog(CWindow /*focus*/, LPARAM /*init_param*/) {
  address_ = base::SysNativeMBToWide(config_->remote_address_).c_str();
  port_ = config_->remote_port_;

  DoDataExchange();

  port_spin_.SetRange32(0, 65535);
  use_ssl_check_.SetCheck(config_->remote_ssl_);
  use_udp_check_.SetCheck(config_->remote_udp_);

  return TRUE;
}

void ScissorsDialog::OnUseSsl(UINT /*notify_code*/, int /*id*/,
                              CWindow /*control*/) {
  if (use_ssl_check_.GetCheck())
    use_udp_check_.SetCheck(FALSE);
}

void ScissorsDialog::OnUseUdp(UINT /*notify_code*/, int /*id*/,
                              CWindow /*control*/) {
  if (use_udp_check_.GetCheck())
    use_ssl_check_.SetCheck(FALSE);
}

void ScissorsDialog::OnOk(UINT /*notify_code*/, int /*id*/,
                          CWindow /*control*/) {
  EDITBALLOONTIP balloon{sizeof(balloon)};
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

  config_->remote_address_ = base::SysWideToNativeMB(address_.GetString());
  config_->remote_port_ = port_;
  config_->remote_ssl_ = use_ssl_check_.GetCheck() != FALSE;
  config_->remote_udp_ = use_udp_check_.GetCheck() != FALSE;

  EndDialog(IDOK);
}

void ScissorsDialog::OnCancel(UINT /*notify_code*/, int /*id*/,
                              CWindow /*control*/) {
  EndDialog(IDCANCEL);
}

}  // namespace ui
}  // namespace scissors
}  // namespace service
}  // namespace juno
