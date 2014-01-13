// Copyright (c) 2013 dacci.org

#include "ui/servers_page.h"

#include <memory>
#include <utility>

#include "app/server_config.h"
#include "ui/server_dialog.h"

ServersPage::ServersPage(PreferenceDialog* parent,
                         std::map<std::string, ServerConfig*>* configs)
    : parent_(parent), configs_(configs), initialized_() {
}

ServersPage::~ServersPage() {
}

void ServersPage::OnPageRelease() {
  delete this;
}

void ServersPage::AddServerItem(ServerConfig* entry, int index) {
  if (index < 0)
    index = server_list_.GetItemCount();

  CString name(entry->name_.c_str());
  CString bind(entry->bind_.c_str());
  CString service_name(entry->service_name_.c_str());

  CString listen;
  listen.Format(_T("%u"), entry->listen_);

  server_list_.InsertItem(index, name);
  server_list_.AddItem(index, 1, bind);
  server_list_.AddItem(index, 2, listen);
  server_list_.AddItem(index, 3, service_name);
  server_list_.SetCheckState(index, entry->enabled_);
  server_list_.SetItemData(index, reinterpret_cast<DWORD_PTR>(entry));
}

BOOL ServersPage::OnInitDialog(CWindow focus, LPARAM init_param) {
  DoDataExchange();

  CString caption;

  server_list_.SetExtendedListViewStyle(LVS_EX_CHECKBOXES |
                                        LVS_EX_FULLROWSELECT);

  caption.LoadString(IDS_COLUMN_NAME);
  server_list_.AddColumn(caption, 0);
  server_list_.SetColumnWidth(0, 100);

  caption.LoadString(IDS_COLUMN_BIND);
  server_list_.AddColumn(caption, 1);
  server_list_.SetColumnWidth(1, 80);

  caption.LoadString(IDS_COLUMN_LISTEN);
  server_list_.AddColumn(caption, 2);
  server_list_.SetColumnWidth(2, 50);

  caption.LoadString(IDS_COLUMN_SERVICE);
  server_list_.AddColumn(caption, 3);
  server_list_.SetColumnWidth(3, 100);

  for (auto i = configs_->begin(), l = configs_->end(); i != l; ++i)
    AddServerItem(i->second, -1);

  edit_button_.EnableWindow(FALSE);
  delete_button_.EnableWindow(FALSE);

  initialized_ = true;

  return TRUE;
}

void ServersPage::OnAddServer(UINT notify_code, int id, CWindow control) {
  std::unique_ptr<ServerConfig> entry(new ServerConfig());
  ServerDialog dialog(parent_, entry.get());
  if (dialog.DoModal(m_hWnd) != IDOK)
    return;

  configs_->insert(std::make_pair(entry->name_, entry.get()));
  AddServerItem(entry.get(), -1);
  server_list_.SelectItem(server_list_.GetItemCount());

  entry.release();
}

void ServersPage::OnEditServer(UINT notify_code, int id, CWindow control) {
  int index = server_list_.GetSelectedIndex();
  ServerConfig* config =
      reinterpret_cast<ServerConfig*>(server_list_.GetItemData(index));

  ServerDialog dialog(parent_, config);
  if (dialog.DoModal(m_hWnd) != IDOK)
    return;

  server_list_.DeleteItem(index);
  AddServerItem(config, index);
  server_list_.SelectItem(index);
}

void ServersPage::OnDeleteServer(UINT notify_code, int id, CWindow control) {
  int index = server_list_.GetSelectedIndex();
  ServerConfig* config =
      reinterpret_cast<ServerConfig*>(server_list_.GetItemData(index));

  server_list_.DeleteItem(index);
  configs_->erase(config->name_);
  delete config;

  server_list_.SelectItem(index);
}

LRESULT ServersPage::OnServerListChanged(LPNMHDR header) {
  if (!initialized_)
    return 0;

  NMLISTVIEW* notify = reinterpret_cast<NMLISTVIEW*>(header);

  if (notify->uNewState & LVIS_STATEIMAGEMASK &&
      notify->uOldState & LVIS_STATEIMAGEMASK) {
    DWORD_PTR item_data = server_list_.GetItemData(notify->lParam);
    ServerConfig* config = reinterpret_cast<ServerConfig*>(item_data);
    if (config != NULL)
      config->enabled_ = (notify->uNewState >> 12) - 1;
  }

  UINT count = server_list_.GetSelectedCount();
  edit_button_.EnableWindow(count > 0);
  delete_button_.EnableWindow(count > 0);

  return 0;
}

LRESULT ServersPage::OnServerListDoubleClicked(LPNMHDR header) {
  NMITEMACTIVATE* notify = reinterpret_cast<NMITEMACTIVATE*>(header);

  if (notify->iItem < 0)
    return 0;

  SendMessage(WM_COMMAND, MAKEWPARAM(IDC_EDIT_BUTTON, 0));

  return 0;
}
