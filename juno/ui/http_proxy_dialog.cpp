// Copyright (c) 2013 dacci.org

#include "ui/http_proxy_dialog.h"

HttpProxyDialog::HttpProxyDialog() {
}

HttpProxyDialog::~HttpProxyDialog() {
}

BOOL HttpProxyDialog::OnInitDialog(CWindow focus, LPARAM init_param) {
  DoDataExchange();

  port_spin_.SetRange32(1, 65535);

  return TRUE;
}

void HttpProxyDialog::OnOk(UINT notify_code, int id, CWindow control) {
  EndDialog(IDOK);
}

void HttpProxyDialog::OnCancel(UINT notify_code, int id, CWindow control) {
  EndDialog(IDCANCEL);
}
