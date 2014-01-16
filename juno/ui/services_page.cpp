// Copyright (c) 2013 dacci.org

#include "ui/services_page.h"

#include <memory>
#include <utility>

#include "app/service_config.h"
#include "app/service_provider.h"
#include "misc/string_util.h"
#include "ui/provider_dialog.h"

ServicesPage::ServicesPage(PreferenceDialog* parent,
                           std::map<std::string, ServiceConfig*>* configs)
    : parent_(parent), configs_(configs), initialized_() {
}

ServicesPage::~ServicesPage() {
}

void ServicesPage::OnPageRelease() {
  delete this;
}

void ServicesPage::AddServiceItem(ServiceConfig* entry, int index) {
  if (index == -1)
    index = service_list_.GetItemCount();

  service_list_.InsertItem(index, CString(entry->name_.c_str()));
  service_list_.AddItem(index, 1, CString(entry->provider_name_.c_str()));
  service_list_.SetItemData(index, reinterpret_cast<DWORD_PTR>(entry));
}

BOOL ServicesPage::OnInitDialog(CWindow focus, LPARAM init_param) {
  DoDataExchange();

  CString caption;

  service_list_.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);

  caption.LoadString(IDS_COLUMN_NAME);
  service_list_.AddColumn(caption, 0);
  service_list_.SetColumnWidth(0, 220);

  caption.LoadString(IDS_COLUMN_PROVIDER);
  service_list_.AddColumn(caption, 1);
  service_list_.SetColumnWidth(1, 90);

  for (auto i = configs_->begin(), l = configs_->end(); i != l; ++i)
    AddServiceItem(i->second, -1);

  edit_button_.EnableWindow(FALSE);
  delete_button_.EnableWindow(FALSE);

  initialized_ = true;

  return TRUE;
}

void ServicesPage::OnAddService(UINT notify_code, int id, CWindow control) {
  ProviderDialog provider_dialog(parent_);
  if (provider_dialog.DoModal(m_hWnd) != IDOK)
    return;

  ServiceProvider* provider = provider_dialog.GetProvider();
  if (provider == NULL)
    return;

  std::unique_ptr<ServiceConfig> config(provider->CreateConfig());
  config->name_ = provider_dialog.name();
  config->provider_name_ = provider_dialog.GetProviderName();
  config->provider_ = provider;

  INT_PTR dialog_result = provider->Configure(config.get(), *parent_);
  if (dialog_result != IDOK)
    return;

  configs_->insert(std::make_pair(config->name_, config.get()));
  AddServiceItem(config.get(), -1);
  service_list_.SelectItem(service_list_.GetItemCount());

  config.release();
}

void ServicesPage::OnEditService(UINT notify_code, int id, CWindow control) {
  int index = service_list_.GetSelectedIndex();
  ServiceConfig* config = reinterpret_cast<ServiceConfig*>(
      service_list_.GetItemData(index));

  if (config->provider_->Configure(config, *parent_) != IDOK)
    return;

  service_list_.DeleteItem(index);
  AddServiceItem(config, index);
  service_list_.SelectItem(index);
}

void ServicesPage::OnDeleteService(UINT notify_code, int id, CWindow control) {
  int index = service_list_.GetSelectedIndex();
  ServiceConfig* config = reinterpret_cast<ServiceConfig*>(
      service_list_.GetItemData(index));

  service_list_.DeleteItem(index);
  configs_->erase(config->name_);
  delete config;

  service_list_.SelectItem(index);
}

LRESULT ServicesPage::OnServiceListChanged(LPNMHDR header) {
  if (!initialized_)
    return 0;

  UINT count = service_list_.GetSelectedCount();
  edit_button_.EnableWindow(count > 0);
  delete_button_.EnableWindow(count > 0);

  return 0;
}

LRESULT ServicesPage::OnServiceListDoubleClicked(LPNMHDR header) {
  NMITEMACTIVATE* notify = reinterpret_cast<NMITEMACTIVATE*>(header);

  if (notify->iItem < 0)
    return 0;

  SendMessage(WM_COMMAND, MAKEWPARAM(IDC_EDIT_BUTTON, 0));

  return 0;
}
