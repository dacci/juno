// Copyright (c) 2013 dacci.org

#include "ui/socks_proxy_dialog.h"

SocksProxyDialog::SocksProxyDialog() {
}

SocksProxyDialog::~SocksProxyDialog() {
}

BOOL SocksProxyDialog::OnInitDialog(CWindow focus, LPARAM init_param) {
  DoDataExchange();
  return TRUE;
}

void SocksProxyDialog::OnOk(UINT notify_code, int id, CWindow control) {
  EndDialog(IDOK);
}

void SocksProxyDialog::OnCancel(UINT notify_code, int id, CWindow control) {
  EndDialog(IDCANCEL);
}
