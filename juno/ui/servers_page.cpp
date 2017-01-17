// Copyright (c) 2013 dacci.org

#include "ui/servers_page.h"

#include <atlstr.h>

#include <base/strings/string_number_conversions.h>
#include <base/strings/sys_string_conversions.h>

#include <memory>
#include <utility>

#include "misc/string_util.h"
#include "ui/preference_dialog.h"
#include "ui/server_dialog.h"

namespace juno {
namespace ui {
namespace {

const wchar_t* kTypeNames[] = {
    L"TCP", L"UDP", L"SSL/TLS",
};

}  // namespace

using ::juno::service::ServerConfig;

ServersPage::ServersPage(PreferenceDialog* parent,
                         service::ServerConfigMap* configs)
    : parent_(parent), configs_(configs), initialized_() {}

void ServersPage::OnPageRelease() {
  delete this;
}

void ServersPage::AddServerItem(const ServerConfig* config, int index) {
  if (index < 0)
    index = server_list_.GetItemCount();

  server_list_.InsertItem(index,
                          base::SysNativeMBToWide(config->bind_).c_str());
  server_list_.AddItem(index, 1, base::IntToString16(config->listen_).c_str());
  server_list_.AddItem(index, 2, kTypeNames[config->type_ - 1]);
  server_list_.AddItem(index, 3, parent_->GetServiceName(config->service_));
  server_list_.SetCheckState(index, config->enabled_);
  server_list_.SetItemData(index, reinterpret_cast<DWORD_PTR>(config));
}

BOOL ServersPage::OnInitDialog(CWindow /*focus*/, LPARAM /*init_param*/) {
  DoDataExchange();

  CString caption;

  server_list_.SetExtendedListViewStyle(LVS_EX_CHECKBOXES |
                                        LVS_EX_FULLROWSELECT);

  caption.LoadString(IDS_COLUMN_BIND);
  server_list_.AddColumn(caption, 0);
  server_list_.SetColumnWidth(0, 100);

  caption.LoadString(IDS_COLUMN_LISTEN);
  server_list_.AddColumn(caption, 1, -1, LVCF_FMT | LVCF_TEXT, LVCFMT_RIGHT);
  server_list_.SetColumnWidth(1, 50);

  caption.LoadString(IDS_COLUMN_PROTOCOL);
  server_list_.AddColumn(caption, 2);
  server_list_.SetColumnWidth(2, 60);

  caption.LoadString(IDS_COLUMN_SERVICE);
  server_list_.AddColumn(caption, 3);
  server_list_.SetColumnWidth(3, 100);

  for (auto& config : *configs_)
    AddServerItem(config.second.get(), -1);

  edit_button_.EnableWindow(FALSE);
  delete_button_.EnableWindow(FALSE);

  initialized_ = true;

  return TRUE;
}

void ServersPage::OnAddServer(UINT /*notify_code*/, int /*id*/,
                              CWindow /*control*/) {
  auto config = std::make_unique<ServerConfig>();
  config->enabled_ = TRUE;

  ServerDialog dialog(parent_, config.get());
  if (dialog.DoModal(m_hWnd) != IDOK)
    return;

  config->id_ = misc::GenerateGUID16();

  AddServerItem(config.get(), -1);
  configs_->insert({config->id_, std::move(config)});

  server_list_.SelectItem(server_list_.GetItemCount());
}

void ServersPage::OnEditServer(UINT /*notify_code*/, int /*id*/,
                               CWindow /*control*/) {
  auto index = server_list_.GetSelectedIndex();
  if (index == CB_ERR)
    return;

  auto config =
      reinterpret_cast<ServerConfig*>(server_list_.GetItemData(index));
  ServerDialog dialog(parent_, config);
  if (dialog.DoModal(m_hWnd) != IDOK)
    return;

  server_list_.DeleteItem(index);
  AddServerItem(config, index);
  server_list_.SelectItem(index);
}

void ServersPage::OnDeleteServer(UINT /*notify_code*/, int /*id*/,
                                 CWindow /*control*/) {
  auto index = server_list_.GetSelectedIndex();
  if (index == CB_ERR)
    return;

  auto config =
      reinterpret_cast<ServerConfig*>(server_list_.GetItemData(index));

  server_list_.DeleteItem(index);
  configs_->erase(config->id_);

  server_list_.SelectItem(index);
}

LRESULT ServersPage::OnServerListChanged(LPNMHDR header) {
  if (!initialized_)
    return 0;

  auto notify = reinterpret_cast<NMLISTVIEW*>(header);

  if (notify->uNewState & LVIS_STATEIMAGEMASK &&
      notify->uOldState & LVIS_STATEIMAGEMASK) {
    auto config = reinterpret_cast<ServerConfig*>(
        server_list_.GetItemData(notify->iItem));
    if (config != nullptr)
      config->enabled_ = (notify->uNewState >> 12) - 1;
  }

  auto count = server_list_.GetSelectedCount();
  edit_button_.EnableWindow(count > 0);
  delete_button_.EnableWindow(count > 0);

  return 0;
}

LRESULT ServersPage::OnServerListDoubleClicked(LPNMHDR header) {
  auto notify = reinterpret_cast<NMITEMACTIVATE*>(header);

  if (notify->iItem < 0)
    return 0;

  SendMessage(WM_COMMAND, MAKEWPARAM(IDC_EDIT_BUTTON, 0));

  return 0;
}

}  // namespace ui
}  // namespace juno
