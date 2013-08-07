// Copyright (c) 2013 dacci.org

#include "ui/services_page.h"

#include <utility>

ServicesPage::ServicesPage() {
  CRegKey services_key;
  services_key.Open(HKEY_CURRENT_USER, "Software\\dacci.org\\Juno\\Services");

  for (DWORD i = 0; ; ++i) {
    ServiceEntry entry;

    ULONG length = 64;
    LONG result = services_key.EnumKey(i, entry.name.GetBuffer(length),
                                       &length);
    if (result != ERROR_SUCCESS)
      break;
    entry.name.ReleaseBuffer(length);

    CRegKey service_key;
    service_key.Open(services_key, entry.name);

    length = 64;
    service_key.QueryStringValue("Provider", entry.provider.GetBuffer(length),
                                 &length);
    entry.provider.ReleaseBuffer(length);

    services_.push_back(std::move(entry));
  }
}

ServicesPage::~ServicesPage() {
}

void ServicesPage::OnPageRelease() {
  delete this;
}

BOOL ServicesPage::OnInitDialog(CWindow focus, LPARAM init_param) {
  DoDataExchange();

  CString caption;

  service_list_.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);

  caption.LoadString(IDS_COLUMN_NAME);
  service_list_.AddColumn(caption, 0);

  caption.LoadString(IDS_COLUMN_PROVIDER);
  service_list_.AddColumn(caption, 1);

  for (auto i = services_.begin(), l = services_.end(); i != l; ++i) {
    int index = service_list_.GetItemCount();
    index = service_list_.InsertItem(index, i->name);
    service_list_.AddItem(index, 1, i->provider);
  }

  service_list_.SetColumnWidth(0, LVSCW_AUTOSIZE);
  service_list_.SetColumnWidth(1, LVSCW_AUTOSIZE);

  edit_button_.EnableWindow(FALSE);
  delete_button_.EnableWindow(FALSE);

  return TRUE;
}

void ServicesPage::OnAddServer(UINT notify_code, int id, CWindow control) {
}

void ServicesPage::OnEditServer(UINT notify_code, int id, CWindow control) {
}

void ServicesPage::OnDeleteServer(UINT notify_code, int id, CWindow control) {
}

LRESULT ServicesPage::OnServiceListClicked(LPNMHDR header) {
  UINT count = service_list_.GetSelectedCount();

  edit_button_.EnableWindow(count > 0);
  delete_button_.EnableWindow(count > 0);

  return 0;
}
