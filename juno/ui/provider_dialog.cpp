// Copyright (c) 2013 dacci.org

#include "ui/provider_dialog.h"

ProviderDialog::ProviderDialog(PreferenceDialog* parent,
                               PreferenceDialog::ServiceEntry* entry)
    : parent_(parent), entry_(entry) {
}

ProviderDialog::~ProviderDialog() {
}

BOOL ProviderDialog::OnInitDialog(CWindow focus, LPARAM init_param) {
  DoDataExchange();

  provider_combo_.AddString("HttpProxy");
  provider_combo_.AddString("SocksProxy");

  return TRUE;
}

void ProviderDialog::OnOk(UINT notify_code, int id, CWindow control) {
  DoDataExchange(DDX_SAVE);

  name_edit_.GetWindowText(entry_->name);
  provider_combo_.GetWindowText(entry_->provider);

  if (entry_->name.IsEmpty()) {
    CStringW message;
    message.LoadString(IDS_NAME_NOT_SPECIFIED);

    EDITBALLOONTIP balloon = { sizeof(balloon) };
    balloon.pszText = message;

    name_edit_.ShowBalloonTip(&balloon);
    return;
  }

  for (auto i = parent_->services_.begin(), l = parent_->services_.end();
       i != l; ++i) {
    if (entry_->name.Compare(i->name) == 0) {
      CStringW message;
      message.LoadString(IDS_DUPLICATE_NAME);

      EDITBALLOONTIP balloon = { sizeof(balloon) };
      balloon.pszText = message;

      name_edit_.ShowBalloonTip(&balloon);
      return;
    }
  }

  EndDialog(provider_combo_.GetCurSel());
}

void ProviderDialog::OnCancel(UINT notify_code, int id, CWindow control) {
  EndDialog(-1);
}
