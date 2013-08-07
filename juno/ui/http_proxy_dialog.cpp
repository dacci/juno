// Copyright (c) 2013 dacci.org

#include "ui/http_proxy_dialog.h"

HttpProxyDialog::HttpProxyDialog() {
}

HttpProxyDialog::~HttpProxyDialog() {
}

BOOL HttpProxyDialog::OnInitDialog(CWindow focus, LPARAM init_param) {
  DoDataExchange();
  return TRUE;
}

void HttpProxyDialog::OnOk(UINT notify_code, int id, CWindow control) {
  EndDialog(IDOK);
}

void HttpProxyDialog::OnCancel(UINT notify_code, int id, CWindow control) {
  EndDialog(IDCANCEL);
}
