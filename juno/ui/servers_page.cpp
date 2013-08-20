// Copyright (c) 2013 dacci.org

#include "ui/servers_page.h"

#include <utility>

#include "ui/server_dialog.h"

ServersPage::ServersPage(PreferenceDialog* parent)
    : parent_(parent), initialized_() {
}

ServersPage::~ServersPage() {
}

void ServersPage::OnPageRelease() {
  delete this;
}

void ServersPage::AddServerItem(const PreferenceDialog::ServerEntry& entry,
                                int index) {
  if (index < 0)
    index = server_list_.GetItemCount();

  CString listen;
  listen.Format(_T("%u"), entry.listen);

  server_list_.InsertItem(index, NULL);
  server_list_.SetCheckState(index, entry.enabled);
  server_list_.AddItem(index, 1, entry.name);
  server_list_.AddItem(index, 2, entry.bind);
  server_list_.AddItem(index, 3, listen);
  server_list_.AddItem(index, 4, entry.service);
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

  for (auto i = parent_->servers_.begin(), l = parent_->servers_.end();
       i != l; ++i)
    AddServerItem(*i, -1);

  if (!parent_->servers_.empty()) {
    server_list_.SetColumnWidth(0, LVSCW_AUTOSIZE);
    server_list_.SetColumnWidth(1, LVSCW_AUTOSIZE);
    server_list_.SetColumnWidth(4, LVSCW_AUTOSIZE);
  }

  edit_button_.EnableWindow(FALSE);
  delete_button_.EnableWindow(FALSE);

  initialized_ = true;

  return TRUE;
}

void ServersPage::OnAddServer(UINT notify_code, int id, CWindow control) {
  PreferenceDialog::ServerEntry entry;
  ServerDialog dialog(parent_, &entry);
  if (dialog.DoModal(m_hWnd) != IDOK)
    return;

  parent_->servers_.push_back(std::move(entry));
  AddServerItem(parent_->servers_.back(), -1);
  server_list_.SelectItem(server_list_.GetItemCount());
}

void ServersPage::OnEditServer(UINT notify_code, int id, CWindow control) {
  int index = server_list_.GetSelectedIndex();
  PreferenceDialog::ServerEntry& entry = parent_->servers_[index];
  ServerDialog dialog(parent_, &entry);
  if (dialog.DoModal(m_hWnd) != IDOK)
    return;

  server_list_.DeleteItem(index);
  AddServerItem(entry, index);
  server_list_.SelectItem(index);
}

void ServersPage::OnDeleteServer(UINT notify_code, int id, CWindow control) {
  int index = server_list_.GetSelectedIndex();
  server_list_.DeleteItem(index);
  parent_->servers_.erase(parent_->servers_.begin() + index);
}

LRESULT ServersPage::OnServerListChanged(LPNMHDR header) {
  if (!initialized_)
    return 0;

  NMLISTVIEW* notify = reinterpret_cast<NMLISTVIEW*>(header);

  if (notify->uNewState & LVIS_STATEIMAGEMASK &&
      notify->uOldState & LVIS_STATEIMAGEMASK)
    parent_->servers_[notify->iItem].enabled = (notify->uNewState >> 12) - 1;

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
