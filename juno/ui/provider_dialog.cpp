// Copyright (c) 2013 dacci.org

#include "ui/provider_dialog.h"

#include <base/strings/string_util.h>

#include <string>

#include "service/service_manager.h"
#include "ui/preference_dialog.h"

namespace juno {
namespace ui {

ProviderDialog::ProviderDialog(PreferenceDialog* parent)
    : parent_(parent), provider_index_(CB_ERR) {}

const std::wstring& ProviderDialog::GetProviderName() const {
  if (provider_index_ == CB_ERR)
    return base::EmptyString16();

  return provider_names_.at(provider_index_);
}

BOOL ProviderDialog::OnInitDialog(CWindow /*focus*/, LPARAM /*init_param*/) {
  DoDataExchange();

  for (auto& provider : service::ServiceManager::GetInstance()->providers()) {
    provider_names_.push_back(provider.first);
    provider_combo_.AddString(provider.first.c_str());
  }

  return TRUE;
}

void ProviderDialog::OnOk(UINT /*notify_code*/, int /*id*/,
                          CWindow /*control*/) {
  HideBalloonTip();

  EDITBALLOONTIP balloon{sizeof(balloon)};
  CString message;

  DoDataExchange(DDX_SAVE);

  if (name_.IsEmpty()) {
    message.LoadString(IDS_NAME_NOT_SPECIFIED);
    balloon.pszText = message;
    name_edit_.ShowBalloonTip(&balloon);
    return;
  }

  for (const auto& pair : parent_->service_configs_) {
    auto new_name = name();
    if (pair.second->name_ == new_name) {
      message.LoadString(IDS_DUPLICATE_NAME);
      balloon.pszText = message;
      name_edit_.ShowBalloonTip(&balloon);
      return;
    }
  }

  provider_index_ = provider_combo_.GetCurSel();
  if (provider_index_ == CB_ERR) {
    ShowBalloonTip(provider_combo_, IDS_NOT_SPECIFIED);
    return;
  }

  EndDialog(IDOK);
}

void ProviderDialog::OnCancel(UINT /*notify_code*/, int /*id*/,
                              CWindow /*control*/) {
  EndDialog(IDCANCEL);
}

}  // namespace ui
}  // namespace juno
