// Copyright (c) 2013 dacci.org

#include "ui/servers_page.h"

#include <assert.h>

#include <memory>
#include <utility>

#include "app/server_config.h"
#include "ui/server_dialog.h"

ServersPage::ServersPage(PreferenceDialog* parent, ServerConfigMap* configs)
    : parent_(parent), configs_(configs), initialized_() {
}

ServersPage::~ServersPage() {
}

void ServersPage::OnPageRelease() {
  delete this;
}

void ServersPage::AddServerItem(const ServerConfigPtr& config, int index) {
  if (index < 0)
    index = server_list_.GetItemCount();

  CString name(config->name_.c_str());
  CString bind(config->bind_.c_str());
  CString service_name(config->service_name_.c_str());

  CString listen;
  listen.Format(_T("%u"), config->listen_);

  server_list_.InsertItem(index, name);
  server_list_.AddItem(index, 1, bind);
  server_list_.AddItem(index, 2, listen);
  server_list_.AddItem(index, 3, service_name);
  server_list_.SetCheckState(index, config->enabled_);
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
  server_list_.AddColumn(caption, 2, -1, LVCF_FMT | LVCF_TEXT, LVCFMT_RIGHT);
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
  ServerConfigPtr config = std::make_shared<ServerConfig>();
  ServerDialog dialog(parent_, config.get());
  if (dialog.DoModal(m_hWnd) != IDOK)
    return;

  configs_->insert(std::make_pair(config->name_, config));
  AddServerItem(config, -1);
  server_list_.SelectItem(server_list_.GetItemCount());
}

void ServersPage::OnEditServer(UINT notify_code, int id, CWindow control) {
  int index = server_list_.GetSelectedIndex();
  if (index == CB_ERR)
    return;

  CString name_unicode;
  server_list_.GetItemText(index, 0, name_unicode);

  CStringA name(name_unicode);
  auto& config = configs_->at(name.GetString());

  ServerDialog dialog(parent_, config.get());
  if (dialog.DoModal(m_hWnd) != IDOK)
    return;

  server_list_.DeleteItem(index);
  AddServerItem(config, index);
  server_list_.SelectItem(index);
}

void ServersPage::OnDeleteServer(UINT notify_code, int id, CWindow control) {
  int index = server_list_.GetSelectedIndex();
  if (index == CB_ERR)
    return;

  CString name_unicode;
  server_list_.GetItemText(index, 0, name_unicode);

  CStringA name(name_unicode);
  auto& config = configs_->at(name.GetString());

  server_list_.DeleteItem(index);
  configs_->erase(config->name_);

  server_list_.SelectItem(index);
}

LRESULT ServersPage::OnServerListChanged(LPNMHDR header) {
  if (!initialized_)
    return 0;

  NMLISTVIEW* notify = reinterpret_cast<NMLISTVIEW*>(header);

  if (notify->uNewState & LVIS_STATEIMAGEMASK &&
      notify->uOldState & LVIS_STATEIMAGEMASK) {
    CString name_unicode;
    server_list_.GetItemText(notify->iItem, 0, name_unicode);

    CStringA name(name_unicode);
    auto& config = configs_->at(name.GetString());

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
