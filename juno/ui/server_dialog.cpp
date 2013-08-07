// Copyright (c) 2013 dacci.org

#include "ui/server_dialog.h"

ServerDialog::ServerDialog() : listen_() {
}

ServerDialog::~ServerDialog() {
}

BOOL ServerDialog::OnInitDialog(CWindow focus, LPARAM init_param) {
  DoDataExchange();

  listen_spin_.SetRange32(1, 65535);

  return TRUE;
}

void ServerDialog::OnOk(UINT notify_code, int id, CWindow control) {
  DoDataExchange(DDX_SAVE);

  EndDialog(IDOK);
}

void ServerDialog::OnCancel(UINT notify_code, int id, CWindow control) {
  EndDialog(IDCANCEL);
}
