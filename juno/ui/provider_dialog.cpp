// Copyright (c) 2013 dacci.org

#include "ui/provider_dialog.h"

#include "app/service_manager.h"
#include "misc/string_util.h"

namespace {
std::string kEmptyString;
}  // namespace

ProviderDialog::ProviderDialog(PreferenceDialog* parent)
    : parent_(parent), provider_index_(CB_ERR) {
}

ProviderDialog::~ProviderDialog() {
}

const std::string& ProviderDialog::GetProviderName() const {
  if (provider_index_ == CB_ERR)
    return kEmptyString;

  return provider_names_.at(provider_index_);
}

BOOL ProviderDialog::OnInitDialog(CWindow focus, LPARAM init_param) {
  DoDataExchange();

  for (auto& provider : service_manager->providers()) {
    provider_names_.push_back(provider.first);
    provider_combo_.AddString(CString(provider.first.c_str()));
  }

  return TRUE;
}

void ProviderDialog::OnOk(UINT notify_code, int id, CWindow control) {
  EDITBALLOONTIP balloon = { sizeof(balloon) };
  CStringW message;

  DoDataExchange(DDX_SAVE);

  if (name_edit_.GetWindowTextLength() <= 0) {
    message.LoadString(IDS_NAME_NOT_SPECIFIED);
    balloon.pszText = message;
    name_edit_.ShowBalloonTip(&balloon);
    return;
  }

  name_.resize(::GetWindowTextLengthA(name_edit_));
  ::GetWindowTextA(name_edit_, &name_[0], name_.size() + 1);

  if (service_manager->GetServiceConfig(name_)) {
    message.LoadString(IDS_DUPLICATE_NAME);
    balloon.pszText = message;
    name_edit_.ShowBalloonTip(&balloon);
    return;
  }

  provider_index_ = provider_combo_.GetCurSel();

  EndDialog(IDOK);
}

void ProviderDialog::OnCancel(UINT notify_code, int id, CWindow control) {
  EndDialog(IDCANCEL);
}
