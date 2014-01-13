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

  return provider_names_[provider_index_];
}

ServiceProvider* ProviderDialog::GetProvider() const {
  if (provider_index_ == CB_ERR)
    return NULL;

  return providers_[provider_index_];
}

BOOL ProviderDialog::OnInitDialog(CWindow focus, LPARAM init_param) {
  DoDataExchange();

  auto& providers = service_manager->providers();
  for (auto i = providers.begin(), l = providers.end(); i != l; ++i) {
    provider_names_.push_back(i->first);
    provider_combo_.AddString(CString(i->first.c_str()));
    providers_.push_back(i->second);
  }

  return TRUE;
}

void ProviderDialog::OnOk(UINT notify_code, int id, CWindow control) {
  EDITBALLOONTIP balloon = { sizeof(balloon) };
  CString message;

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
