// Copyright (c) 2013 dacci.org

#include "ui/http_proxy_dialog.h"

#include "ui/http_header_filter_dialog.h"

static const char* kActions[] = {
  "Set",
  "Append",
  "Add",
  "Unset",
  "Merge",
  "Edit",
  "Edit*",
};

HttpProxyDialog::HttpProxyDialog(PreferenceDialog::ServiceEntry* entry)
    : config_(static_cast<PreferenceDialog::HttpProxyEntry*>(entry->extra)) {
  image_list_.CreateFromImage(IDB_UPDOWN_ARROW, 16, 0, CLR_NONE, 0,
                              LR_CREATEDIBSECTION);
}

HttpProxyDialog::~HttpProxyDialog() {
}

void HttpProxyDialog::AddFilterItem(
    const PreferenceDialog::HttpHeaderFilter& filter, int filter_index,
    int index) {
  if (index == -1)
    index = filter_list_.GetItemCount();

  int image = 0;
  if (filter.request)
    image |= 1;
  if (filter.response)
    image |= 2;

  filter_list_.InsertItem(LVIF_IMAGE | LVIF_PARAM, index, NULL, 0, 0, image - 1,
                          filter_index);
  filter_list_.AddItem(index, 1, kActions[filter.action]);
  filter_list_.AddItem(index, 2, filter.name);
  filter_list_.AddItem(index, 3, filter.value);
  filter_list_.AddItem(index, 4, filter.replace);
}

BOOL HttpProxyDialog::OnInitDialog(CWindow focus, LPARAM init_param) {
  port_ = config_->remote_proxy_port;
  remote_password_ = config_->remote_proxy_password;

  DoDataExchange();

  filter_list_.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);
  filter_list_.SetImageList(image_list_, LVSIL_SMALL);
  filter_list_.AddColumn("", 0);
  filter_list_.SetColumnWidth(0, 24);

  CString caption;

  caption.LoadString(IDS_COLUMN_ACTION);
  filter_list_.AddColumn(caption, 1);

  caption.LoadString(IDS_COLUMN_HEADER);
  filter_list_.AddColumn(caption, 2);
  filter_list_.SetColumnWidth(2, 100);

  caption.LoadString(IDS_COLUMN_VALUE);
  filter_list_.AddColumn(caption, 3);
  filter_list_.SetColumnWidth(3, 100);

  caption.LoadString(IDS_COLUMN_REPLACE);
  filter_list_.AddColumn(caption, 4);
  filter_list_.SetColumnWidth(4, 100);

  CBitmap bitmap;
  bitmap.LoadBitmap(IDB_ARROW_UP);
  up_button_.SetBitmap(bitmap);
  bitmap.DeleteObject();

  bitmap.LoadBitmap(IDB_DOCUMENT_NEW);
  add_button_.SetBitmap(bitmap);
  bitmap.DeleteObject();

  bitmap.LoadBitmap(IDB_DOCUMENT_EDIT);
  edit_button_.SetBitmap(bitmap);
  bitmap.DeleteObject();

  bitmap.LoadBitmap(IDB_DOCUMENT_CLOSE);
  delete_button_.SetBitmap(bitmap);
  bitmap.DeleteObject();

  bitmap.LoadBitmap(IDB_ARROW_DOWN);
  down_button_.SetBitmap(bitmap);
  bitmap.DeleteObject();

  use_remote_proxy_check_.SetCheck(config_->use_remote_proxy);
  address_edit_.SetWindowText(config_->remote_proxy_host);
  port_spin_.SetRange32(1, 65535);
  auth_remote_check_.SetCheck(config_->auth_remote_proxy);
  remote_user_edit_.SetWindowText(config_->remote_proxy_user);

  int filter_index = 0;
  for (auto i = config_->header_filters.begin(),
            l = config_->header_filters.end(); i != l; ++i)
    AddFilterItem(*i, filter_index++, -1);

  return TRUE;
}

void HttpProxyDialog::OnAddFilter(UINT notify_code, int id, CWindow control) {
  PreferenceDialog::HttpHeaderFilter filter = { true };
  HttpHeaderFilterDialog dialog(&filter);
  if (dialog.DoModal(m_hWnd) != IDOK)
    return;

  config_->header_filters.push_back(filter);
  AddFilterItem(filter, config_->header_filters.size() - 1, -1);
  filter_list_.SelectItem(filter_list_.GetItemCount());
}

void HttpProxyDialog::OnEditFilter(UINT notify_code, int id, CWindow control) {
  if (filter_list_.GetSelectedCount() != 1)
    return;

  int index = filter_list_.GetSelectedIndex();
  int filter_index = filter_list_.GetItemData(index);
  PreferenceDialog::HttpHeaderFilter& filter =
      config_->header_filters[filter_index];

  HttpHeaderFilterDialog dialog(&filter);
  if (dialog.DoModal(m_hWnd) != IDOK)
    return;

  filter_list_.DeleteItem(index);
  AddFilterItem(filter, filter_index, index);
  filter_list_.SelectItem(index);
}

void HttpProxyDialog::OnDeleteFilter(UINT notify_code, int id,
                                     CWindow control) {
  if (filter_list_.GetSelectedCount() != 1)
    return;

  int index = filter_list_.GetSelectedIndex();
  int filter_index = filter_list_.GetItemData(index);
  filter_list_.DeleteItem(index);
  config_->header_filters[filter_index].removed = true;
}

void HttpProxyDialog::OnScrollUp(UINT notify_code, int id, CWindow control) {
  if (filter_list_.GetSelectedCount() != 1)
    return;

  // TODO(dacci)
  ATLASSERT(FALSE);
}

void HttpProxyDialog::OnScrollDown(UINT notify_code, int id, CWindow control) {
  if (filter_list_.GetSelectedCount() != 1)
    return;

  // TODO(dacci)
  ATLASSERT(FALSE);
}

void HttpProxyDialog::OnOk(UINT notify_code, int id, CWindow control) {
  DoDataExchange(DDX_SAVE);

  config_->use_remote_proxy = use_remote_proxy_check_.GetCheck();
  address_edit_.GetWindowText(config_->remote_proxy_host);
  config_->remote_proxy_port = port_;
  config_->auth_remote_proxy = auth_remote_check_.GetCheck();
  remote_user_edit_.GetWindowText(config_->remote_proxy_user);
  config_->remote_proxy_password = remote_password_;

  auto& filters = config_->header_filters;
  auto i = filters.begin();
  while (i != filters.end()) {
    if (i->removed) {
      if (i == filters.begin()) {
        filters.erase(i);
        i = filters.begin();
        continue;
      } else {
        filters.erase(i--);
      }
    }

    ++i;
  }

  EndDialog(IDOK);
}

void HttpProxyDialog::OnCancel(UINT notify_code, int id, CWindow control) {
  auto& filters = config_->header_filters;
  auto i = filters.begin();
  while (i != filters.end()) {
    if (i->added) {
      if (i == filters.begin()) {
        filters.erase(i);
        i = filters.begin();
        continue;
      } else {
        filters.erase(i--);
      }
    } else if (i->removed) {
      i->removed = false;
    }

    ++i;
  }

  EndDialog(IDCANCEL);
}

LRESULT HttpProxyDialog::OnFilterListDoubleClicked(LPNMHDR header) {
  NMITEMACTIVATE* notify = reinterpret_cast<NMITEMACTIVATE*>(header);

  if (notify->iItem < 0)
    return 0;

  SendMessage(WM_COMMAND, MAKEWPARAM(IDC_EDIT_BUTTON, 0));

  return 0;
}
