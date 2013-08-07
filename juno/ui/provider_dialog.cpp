// Copyright (c) 2013 dacci.org

#include "ui/provider_dialog.h"

ProviderDialog::ProviderDialog() {
}

ProviderDialog::~ProviderDialog() {
}

BOOL ProviderDialog::OnInitDialog(CWindow focus, LPARAM init_param) {
  DoDataExchange();
  return TRUE;
}

void ProviderDialog::OnOk(UINT notify_code, int id, CWindow control) {
  EndDialog(IDOK);
}

void ProviderDialog::OnCancel(UINT notify_code, int id, CWindow control) {
  EndDialog(IDCANCEL);
}
