// Copyright (c) 2013 dacci.org

#include "ui/http_proxy_dialog.h"

#include <utility>

#include "misc/string_util.h"
#include "ui/http_header_filter_dialog.h"

static const TCHAR* kActions[] = {
  _T("Set"),
  _T("Append"),
  _T("Add"),
  _T("Unset"),
  _T("Merge"),
  _T("Edit"),
  _T("Edit*"),
};

HttpProxyDialog::HttpProxyDialog(const ServiceConfigPtr& entry)
    : config_(std::static_pointer_cast<HttpProxyConfig>(entry)),
      filters_(config_->header_filters_) {
  image_list_.CreateFromImage(IDB_UPDOWN_ARROW, 16, 0, CLR_NONE, 0,
                              LR_CREATEDIBSECTION);
}

HttpProxyDialog::~HttpProxyDialog() {
}

void HttpProxyDialog::AddFilterItem(const HttpProxyConfig::HeaderFilter& filter,
                                    int filter_index, int index) {
  if (index == -1)
    index = filter_list_.GetItemCount();

  int image = 0;
  if (filter.request)
    image |= 1;
  if (filter.response)
    image |= 2;

  filter_list_.InsertItem(LVIF_IMAGE | LVIF_PARAM, index, nullptr, 0, 0,
                          image - 1, filter_index);
  filter_list_.AddItem(index, 1, kActions[filter.action]);
  filter_list_.AddItem(index, 2, CString(filter.name.c_str()));
  filter_list_.AddItem(index, 3, CString(filter.value.c_str()));
  filter_list_.AddItem(index, 4, CString(filter.replace.c_str()));
}

BOOL HttpProxyDialog::OnInitDialog(CWindow focus, LPARAM init_param) {
  port_ = config_->remote_proxy_port_;

  DoDataExchange();

  filter_list_.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);
  filter_list_.SetImageList(image_list_, LVSIL_SMALL);
  filter_list_.AddColumn(nullptr, 0);
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

  up_button_.SetBitmap(::AtlLoadBitmapImage(IDB_ARROW_UP));
  add_button_.SetBitmap(::AtlLoadBitmapImage(IDB_DOCUMENT_NEW));
  edit_button_.SetBitmap(::AtlLoadBitmapImage(IDB_DOCUMENT_EDIT));
  delete_button_.SetBitmap(::AtlLoadBitmapImage(IDB_DOCUMENT_CLOSE));
  down_button_.SetBitmap(::AtlLoadBitmapImage(IDB_ARROW_DOWN));

  use_remote_proxy_check_.SetCheck(config_->use_remote_proxy_);
  address_edit_.SetWindowText(CString(config_->remote_proxy_host_.c_str()));
  port_spin_.SetRange32(0, 65535);
  auth_remote_check_.SetCheck(config_->auth_remote_proxy_);
  remote_user_edit_.SetWindowText(CString(config_->remote_proxy_user_.c_str()));
  remote_password_edit_.SetWindowText(
      CString(config_->remote_proxy_password_.c_str()));

  int filter_index = 0;
  for (auto& filter : filters_)
    AddFilterItem(filter, filter_index++, -1);

  return TRUE;
}

void HttpProxyDialog::OnAddFilter(UINT notify_code, int id, CWindow control) {
  HttpProxyConfig::HeaderFilter filter = { true };
  HttpHeaderFilterDialog dialog(&filter);
  if (dialog.DoModal(m_hWnd) != IDOK)
    return;

  filters_.push_back(filter);
  AddFilterItem(filter, filters_.size() - 1, -1);
  filter_list_.SelectItem(filter_list_.GetItemCount());
}

void HttpProxyDialog::OnEditFilter(UINT notify_code, int id, CWindow control) {
  if (filter_list_.GetSelectedCount() != 1)
    return;

  int index = filter_list_.GetSelectedIndex();
  int filter_index = filter_list_.GetItemData(index);
  HttpProxyConfig::HeaderFilter& filter = filters_[filter_index];

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
  filters_.erase(filters_.begin() + filter_index);
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
  EDITBALLOONTIP balloon = { sizeof(balloon) };
  CStringW message;

  DoDataExchange(DDX_SAVE);

  if (use_remote_proxy_check_.GetCheck()) {
    if (address_edit_.GetWindowTextLength() <= 0) {
      message.LoadString(IDS_NOT_SPECIFIED);
      balloon.pszText = message;
      address_edit_.ShowBalloonTip(&balloon);
      return;
    }

    if (port_ <= 0 || 65536 <= port_) {
      message.LoadString(IDS_INVALID_PORT);
      balloon.pszText = message;
      port_edit_.ShowBalloonTip(&balloon);
      return;
    }
  }

  config_->use_remote_proxy_ = use_remote_proxy_check_.GetCheck();

  std::string temp;

  temp.resize(::GetWindowTextLengthA(address_edit_));
  ::GetWindowTextA(address_edit_, &temp[0], temp.size() + 1);
  config_->remote_proxy_host_ = temp;

  config_->remote_proxy_port_ = port_;
  config_->auth_remote_proxy_ = auth_remote_check_.GetCheck();

  temp.resize(::GetWindowTextLengthA(remote_user_edit_));
  ::GetWindowTextA(remote_user_edit_, &temp[0], temp.size() + 1);
  config_->remote_proxy_user_ = temp;

  temp.resize(::GetWindowTextLengthA(remote_password_edit_));
  ::GetWindowTextA(remote_password_edit_, &temp[0], temp.size() + 1);
  config_->remote_proxy_password_ = temp;

  config_->header_filters_ = std::move(filters_);

  EndDialog(IDOK);
}

void HttpProxyDialog::OnCancel(UINT notify_code, int id, CWindow control) {
  EndDialog(IDCANCEL);
}

LRESULT HttpProxyDialog::OnFilterListDoubleClicked(LPNMHDR header) {
  NMITEMACTIVATE* notify = reinterpret_cast<NMITEMACTIVATE*>(header);

  if (notify->iItem < 0)
    return 0;

  SendMessage(WM_COMMAND, MAKEWPARAM(IDC_EDIT_BUTTON, 0));

  return 0;
}
