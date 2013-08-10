// Copyright (c) 2013 dacci.org

#include "ui/http_proxy_dialog.h"

HttpProxyDialog::HttpProxyDialog(PreferenceDialog::ServiceEntry* entry)
    : entry_(entry) {
}

HttpProxyDialog::~HttpProxyDialog() {
}

BOOL HttpProxyDialog::OnInitDialog(CWindow focus, LPARAM init_param) {
  PreferenceDialog::HttpProxyEntry* proxy_entry =
      static_cast<PreferenceDialog::HttpProxyEntry*>(entry_->extra);

  port_ = proxy_entry->remote_proxy_port;
  remote_password_ = proxy_entry->remote_proxy_password;

  DoDataExchange();

  use_remote_proxy_check_.SetCheck(proxy_entry->use_remote_proxy);
  address_edit_.SetWindowText(proxy_entry->remote_proxy_host);
  port_spin_.SetRange32(1, 65535);
  auth_remote_check_.SetCheck(proxy_entry->auth_remote_proxy);
  remote_user_edit_.SetWindowText(proxy_entry->remote_proxy_user);

  return TRUE;
}

void HttpProxyDialog::OnOk(UINT notify_code, int id, CWindow control) {
  PreferenceDialog::HttpProxyEntry* proxy_entry =
      static_cast<PreferenceDialog::HttpProxyEntry*>(entry_->extra);

  DoDataExchange(DDX_SAVE);

  proxy_entry->use_remote_proxy = use_remote_proxy_check_.GetCheck();
  address_edit_.GetWindowText(proxy_entry->remote_proxy_host);
  proxy_entry->remote_proxy_port = port_;
  proxy_entry->auth_remote_proxy = auth_remote_check_.GetCheck();
  remote_user_edit_.GetWindowText(proxy_entry->remote_proxy_user);
  proxy_entry->remote_proxy_password = remote_password_;

  EndDialog(IDOK);
}

void HttpProxyDialog::OnCancel(UINT notify_code, int id, CWindow control) {
  EndDialog(IDCANCEL);
}
