// Copyright (c) 2013 dacci.org

#include "ui/server_dialog.h"

#include <iphlpapi.h>

ServerDialog::ServerDialog(PreferenceDialog* parent,
                           PreferenceDialog::ServerEntry* entry)
    : entry_(entry), parent_(parent) {
}

ServerDialog::~ServerDialog() {
}

void ServerDialog::FillBindCombo() {
  bind_combo_.Clear();
  bind_combo_.AddString(_T("*"));
  bind_combo_.AddString(_T("localhost"));

  ULONG size = 0;
  ::GetAdaptersAddresses(AF_UNSPEC, 0, NULL, NULL, &size);

  IP_ADAPTER_ADDRESSES* addresses =
      static_cast<IP_ADAPTER_ADDRESSES*>(::malloc(size));
  ULONG error = ::GetAdaptersAddresses(AF_UNSPEC, 0, NULL, addresses, &size);
  ATLASSERT(error == ERROR_SUCCESS);
  if (error != ERROR_SUCCESS)
    return;

  for (auto pointer = addresses; pointer; pointer = pointer->Next) {
    for (auto address = pointer->FirstUnicastAddress; address;
         address = address->Next) {
      CString text;
      DWORD length = 48;
      error = ::WSAAddressToString(address->Address.lpSockaddr,
                                   address->Address.iSockaddrLength,
                                   NULL,
                                   text.GetBuffer(length),
                                   &length);
      ATLASSERT(error == 0);
      if (error != 0)
        continue;

      text.ReleaseBuffer(length);
      bind_combo_.AddString(text);
    }
  }

  ::free(addresses);
}

void ServerDialog::FillServiceCombo() {
  service_combo_.Clear();

  for (auto i = parent_->services_.begin(), l = parent_->services_.end();
       i != l; ++i)
    service_combo_.AddString(i->name);
}

BOOL ServerDialog::OnInitDialog(CWindow focus, LPARAM init_param) {
  listen_ = entry_->listen;

  DoDataExchange();

  name_edit_.SetWindowText(entry_->name);

  FillBindCombo();

  type_combo_.AddString(L"TCP");
  type_combo_.AddString(L"UDP");

  FillServiceCombo();

  bind_combo_.SetWindowText(entry_->bind);
  listen_spin_.SetRange32(0, 65535);
  type_combo_.SetCurSel(entry_->type - SOCK_STREAM);
  service_combo_.SelectString(0, entry_->service);
  enable_check_.SetCheck(entry_->enabled);

  return TRUE;
}

void ServerDialog::OnOk(UINT notify_code, int id, CWindow control) {
  EDITBALLOONTIP balloon = { sizeof(balloon) };
  CString message;

  DoDataExchange(DDX_SAVE);

  if (name_edit_.GetWindowTextLength() == 0) {
    message.LoadString(IDS_NAME_NOT_SPECIFIED);
    balloon.pszText = message.GetString();
    name_edit_.ShowBalloonTip(&balloon);
    return;
  }

  if (listen_ <= 0 || 65536 <= listen_) {
    message.LoadString(IDS_INVALID_PORT);
    balloon.pszText = message.GetString();
    listen_edit_.ShowBalloonTip(&balloon);
    return;
  }

  name_edit_.GetWindowText(entry_->name);
  bind_combo_.GetWindowText(entry_->bind);
  entry_->listen = listen_;
  entry_->type = type_combo_.GetCurSel() + SOCK_STREAM;
  service_combo_.GetWindowText(entry_->service);
  entry_->enabled = enable_check_.GetCheck();

  EndDialog(IDOK);
}

void ServerDialog::OnCancel(UINT notify_code, int id, CWindow control) {
  EndDialog(IDCANCEL);
}
