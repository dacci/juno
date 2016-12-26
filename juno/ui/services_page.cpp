// Copyright (c) 2013 dacci.org

#include "ui/services_page.h"

#include <memory>
#include <utility>

#include "service/service_config.h"
#include "service/service_provider.h"
#include "ui/preference_dialog.h"
#include "ui/provider_dialog.h"

namespace juno {
namespace ui {

ServicesPage::ServicesPage(PreferenceDialog* parent,
                           service::ServiceConfigMap* configs)
    : parent_(parent), configs_(configs), initialized_() {}

void ServicesPage::OnPageRelease() {
  delete this;
}

void ServicesPage::AddServiceItem(const service::ServiceConfigPtr& config,
                                  int index) {
  if (index == -1)
    index = service_list_.GetItemCount();

  service_list_.InsertItem(index, CString(config->name_.c_str()));
  service_list_.AddItem(index, 1, CString(config->provider_name_.c_str()));
}

BOOL ServicesPage::OnInitDialog(CWindow /*focus*/, LPARAM /*init_param*/) {
  DoDataExchange();

  CString caption;

  service_list_.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);

  caption.LoadString(IDS_COLUMN_NAME);
  service_list_.AddColumn(caption, 0);
  service_list_.SetColumnWidth(0, 220);

  caption.LoadString(IDS_COLUMN_PROVIDER);
  service_list_.AddColumn(caption, 1);
  service_list_.SetColumnWidth(1, 90);

  for (auto& pair : *configs_)
    AddServiceItem(pair.second, -1);

  edit_button_.EnableWindow(FALSE);
  delete_button_.EnableWindow(FALSE);

  initialized_ = true;

  return TRUE;
}

void ServicesPage::OnAddService(UINT /*notify_code*/, int /*id*/,
                                CWindow /*control*/) {
  ProviderDialog provider_dialog(parent_);
  if (provider_dialog.DoModal(m_hWnd) != IDOK)
    return;

  auto provider = service::ServiceManager::GetInstance()->GetProvider(
      provider_dialog.GetProviderName());
  if (provider == nullptr)
    return;

  auto config = provider->CreateConfig();
  config->name_ = provider_dialog.name();
  config->provider_name_ = provider_dialog.GetProviderName();

  auto dialog_result = provider->Configure(config, *parent_);
  if (dialog_result != IDOK)
    return;

  configs_->insert(std::make_pair(config->name_, config));
  AddServiceItem(config, -1);
  service_list_.SelectItem(service_list_.GetItemCount());
}

void ServicesPage::OnEditService(UINT /*notify_code*/, int /*id*/,
                                 CWindow /*control*/) {
  auto index = service_list_.GetSelectedIndex();
  if (index == CB_ERR)
    return;

  CString name_unicode;
  service_list_.GetItemText(index, 0, name_unicode);

  CStringA name(name_unicode);
  auto& config = configs_->at(name.GetString());

  auto provider = service::ServiceManager::GetInstance()->GetProvider(
      config->provider_name_);
  if (provider->Configure(config, *parent_) != IDOK)
    return;

  service_list_.DeleteItem(index);
  AddServiceItem(config, index);
  service_list_.SelectItem(index);
}

void ServicesPage::OnDeleteService(UINT /*notify_code*/, int /*id*/,
                                   CWindow /*control*/) {
  auto index = service_list_.GetSelectedIndex();
  if (index == CB_ERR)
    return;

  CString name_unicode;
  service_list_.GetItemText(index, 0, name_unicode);

  CStringA name(name_unicode);
  auto& config = configs_->at(name.GetString());

  service_list_.DeleteItem(index);
  configs_->erase(config->name_);

  service_list_.SelectItem(index);
}

LRESULT ServicesPage::OnServiceListChanged(LPNMHDR /*header*/) {
  if (!initialized_)
    return 0;

  auto count = service_list_.GetSelectedCount();
  edit_button_.EnableWindow(count > 0);
  delete_button_.EnableWindow(count > 0);

  return 0;
}

LRESULT ServicesPage::OnServiceListDoubleClicked(LPNMHDR header) {
  auto notify = reinterpret_cast<NMITEMACTIVATE*>(header);
  if (notify->iItem < 0)
    return 0;

  SendMessage(WM_COMMAND, MAKEWPARAM(IDC_EDIT_BUTTON, 0));

  return 0;
}

}  // namespace ui
}  // namespace juno
