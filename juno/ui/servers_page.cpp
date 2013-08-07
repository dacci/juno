// Copyright (c) 2013 dacci.org

#include "ui/servers_page.h"

#include <utility>

ServersPage::ServersPage() {
  CRegKey servers_key;
  servers_key.Open(HKEY_CURRENT_USER, "Software\\dacci.org\\Juno\\Servers");

  for (DWORD i = 0; ; ++i) {
    ServerEntry entry;

    ULONG length = 64;
    LONG result = servers_key.EnumKey(i, entry.name.GetBuffer(length), &length);
    if (result != ERROR_SUCCESS)
      break;
    entry.name.ReleaseBuffer(length);

    CRegKey server_key;
    server_key.Open(servers_key, entry.name);

    server_key.QueryDWORDValue("Enabled", entry.enabled);

    length = 48;
    server_key.QueryStringValue("Bind", entry.bind.GetBuffer(length), &length);
    entry.bind.ReleaseBuffer(length);

    server_key.QueryDWORDValue("Listen", entry.listen);

    length = 64;
    server_key.QueryStringValue("Service", entry.service.GetBuffer(length),
                                &length);
    entry.service.ReleaseBuffer(length);

    servers_.push_back(std::move(entry));
  }
}

ServersPage::~ServersPage() {
}

void ServersPage::OnPageRelease() {
  delete this;
}

BOOL ServersPage::OnInitDialog(CWindow focus, LPARAM init_param) {
  DoDataExchange();

  CString caption;

  server_list_.SetExtendedListViewStyle(LVS_EX_CHECKBOXES |
                                        LVS_EX_FULLROWSELECT);
  server_list_.AddColumn(caption, 0);

  caption.LoadString(IDS_COLUMN_NAME);
  server_list_.AddColumn(caption, 1);

  caption.LoadString(IDS_COLUMN_BIND);
  server_list_.AddColumn(caption, 2);

  caption.LoadString(IDS_COLUMN_LISTEN);
  server_list_.AddColumn(caption, 3);

  caption.LoadString(IDS_COLUMN_SERVICE);
  server_list_.AddColumn(caption, 4);

  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i) {
    int index = server_list_.GetItemCount();
    index = server_list_.InsertItem(index, NULL);
    server_list_.SetCheckState(index, i->enabled);
    server_list_.AddItem(index, 1, i->name);
    server_list_.AddItem(index, 2, i->bind);
    server_list_.AddItem(index, 4, i->service);

    caption.Format("%u", i->listen);
    server_list_.AddItem(index, 3, caption);
  }

  server_list_.SetColumnWidth(0, LVSCW_AUTOSIZE);
  server_list_.SetColumnWidth(1, LVSCW_AUTOSIZE);
  server_list_.SetColumnWidth(4, LVSCW_AUTOSIZE);

  edit_button_.EnableWindow(FALSE);
  delete_button_.EnableWindow(FALSE);

  return TRUE;
}

void ServersPage::OnAddServer(UINT notify_code, int id, CWindow control) {
}

void ServersPage::OnEditServer(UINT notify_code, int id, CWindow control) {
}

void ServersPage::OnDeleteServer(UINT notify_code, int id, CWindow control) {
}

LRESULT ServersPage::OnServerListClicked(LPNMHDR header) {
  UINT count = server_list_.GetSelectedCount();

  edit_button_.EnableWindow(count > 0);
  delete_button_.EnableWindow(count > 0);

  return 0;
}
