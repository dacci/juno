// Copyright (c) 2013 dacci.org

#include "ui/services_page.h"

#include <utility>

#include "ui/http_proxy_dialog.h"
#include "ui/provider_dialog.h"
#include "ui/socks_proxy_dialog.h"

ServicesPage::ServicesPage(PreferenceDialog* parent)
    : parent_(parent), initialized_() {
}

ServicesPage::~ServicesPage() {
}

void ServicesPage::OnPageRelease() {
  delete this;
}

void ServicesPage::AddServiceItem(const PreferenceDialog::ServiceEntry& entry,
                                  int index) {
  if (index == -1)
    index = service_list_.GetItemCount();

  service_list_.InsertItem(index, entry.name);
  service_list_.AddItem(index, 1, entry.provider);
}

BOOL ServicesPage::OnInitDialog(CWindow focus, LPARAM init_param) {
  DoDataExchange();

  CString caption;

  service_list_.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);

  caption.LoadString(IDS_COLUMN_NAME);
  service_list_.AddColumn(caption, 0);

  caption.LoadString(IDS_COLUMN_PROVIDER);
  service_list_.AddColumn(caption, 1);

  for (auto i = parent_->services_.begin(), l = parent_->services_.end();
       i != l; ++i)
    AddServiceItem(*i, -1);

  if (!parent_->services_.empty()) {
    service_list_.SetColumnWidth(0, LVSCW_AUTOSIZE);
    service_list_.SetColumnWidth(1, LVSCW_AUTOSIZE);
  }

  edit_button_.EnableWindow(FALSE);
  delete_button_.EnableWindow(FALSE);

  initialized_ = true;

  return TRUE;
}

void ServicesPage::OnAddService(UINT notify_code, int id, CWindow control) {
  PreferenceDialog::ServiceEntry entry;

  ProviderDialog provider_dialog(parent_, &entry);
  INT_PTR provider = provider_dialog.DoModal(m_hWnd);
  if (provider < 0)
    return;

  INT_PTR dialog_result = IDCANCEL;

  switch (provider) {
    case 0: {  // HttpProxy
      entry.extra = new PreferenceDialog::HttpProxyEntry();

      HttpProxyDialog dialog(&entry);
      dialog_result = dialog.DoModal(m_hWnd);
      break;
    }
    case 1: {  // SocksProxy
      // no configuration
      dialog_result = IDOK;
      break;
    }
  }

  if (dialog_result != IDOK)
    return;

  parent_->services_.push_back(std::move(entry));
  AddServiceItem(parent_->services_.back(), -1);
}

void ServicesPage::OnEditService(UINT notify_code, int id, CWindow control) {
  int index = service_list_.GetSelectedIndex();
  PreferenceDialog::ServiceEntry& entry = parent_->services_[index];

  INT_PTR dialog_result = IDCANCEL;

  if (entry.provider.Compare("HttpProxy") == 0) {
    HttpProxyDialog dialog(&entry);
    dialog_result = dialog.DoModal(m_hWnd);
  } else if (entry.provider.Compare("SocksProxy") == 0) {
    // no configuration
    dialog_result = IDOK;
  }

  if (dialog_result != IDOK)
    return;

  service_list_.DeleteItem(index);
  AddServiceItem(entry, index);
}

void ServicesPage::OnDeleteService(UINT notify_code, int id, CWindow control) {
  int index = service_list_.GetSelectedIndex();
  service_list_.DeleteItem(index);
  parent_->services_.erase(parent_->services_.begin() + index);
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
