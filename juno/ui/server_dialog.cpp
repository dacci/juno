// Copyright (c) 2013 dacci.org

#include "ui/server_dialog.h"

#include <assert.h>

#include <iphlpapi.h>

#include <string>

#include "app/server_config.h"
#include "misc/certificate_store.h"
#include "misc/string_util.h"

ServerDialog::ServerDialog(PreferenceDialog* parent, ServerConfig* entry)
    : parent_(parent), entry_(entry) {
}

ServerDialog::~ServerDialog() {
}

void ServerDialog::FillBindCombo() {
  bind_combo_.Clear();
  bind_combo_.AddString(_T("*"));
  bind_combo_.AddString(_T("localhost"));

  ULONG size = 0;
  ::GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, nullptr, &size);

  IP_ADAPTER_ADDRESSES* addresses =
      static_cast<IP_ADAPTER_ADDRESSES*>(::malloc(size));
  ULONG error = ::GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, addresses, &size);
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
                                   nullptr,
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

  for (auto& service : parent_->service_configs_)
    service_combo_.AddString(CString(service.first.c_str()));
}

BOOL ServerDialog::OnInitDialog(CWindow focus, LPARAM init_param) {
  bind_ = entry_->bind_.c_str();
  listen_ = entry_->listen_;
  enabled_ = entry_->enabled_;

  DoDataExchange();

  FillBindCombo();

  type_combo_.AddString(_T("TCP"));
  type_combo_.AddString(_T("UDP"));
  type_combo_.AddString(_T("SSL/TLS"));
  detail_button_.EnableWindow(FALSE);

  FillServiceCombo();

  bind_combo_.SetWindowText(CString(entry_->bind_.c_str()));
  listen_spin_.SetRange32(0, 65535);
  type_combo_.SetCurSel(entry_->type_ - 1);

  switch (entry_->type_) {
    case ServerConfig::TCP:
    case ServerConfig::UDP:
      detail_button_.EnableWindow(FALSE);
      break;

    case ServerConfig::TLS:
      detail_button_.EnableWindow(TRUE);
      break;
  }

  service_combo_.SelectString(-1, CString(entry_->service_name_.c_str()));

  return TRUE;
}

void ServerDialog::OnTypeChange(UINT notify_code, int id, CWindow control) {
  switch (type_combo_.GetCurSel() + 1) {
    case ServerConfig::TCP:
    case ServerConfig::UDP:
      detail_button_.EnableWindow(FALSE);
      break;

    case ServerConfig::TLS:
      detail_button_.EnableWindow();
      break;
  }
}

void ServerDialog::OnDetailSetting(UINT notify_code, int id, CWindow control) {
  switch (type_combo_.GetCurSel() + 1) {
    case ServerConfig::TLS: {
      CertificateStore store(L"MY");
      PCCERT_CONTEXT cert =
          store.SelectCertificate(m_hWnd, nullptr, nullptr, 0);
      if (cert == nullptr)
        break;

      DWORD length = 20;
      entry_->cert_hash_.resize(length);
      if (CertGetCertificateContextProperty(cert, CERT_HASH_PROP_ID,
                                            entry_->cert_hash_.data(), &length))
        entry_->cert_hash_.resize(length);
      else
        entry_->cert_hash_.clear();

      CertFreeCertificateContext(cert);

      break;
    }

    default:
      assert(false);
  }
}

void ServerDialog::OnOk(UINT notify_code, int id, CWindow control) {
  EDITBALLOONTIP balloon = { sizeof(balloon) };
  CStringW message;
  std::string temp;

  DoDataExchange(DDX_SAVE);

  if (listen_ <= 0 || 65536 <= listen_) {
    message.LoadString(IDS_INVALID_PORT);
    balloon.pszText = message.GetString();
    listen_edit_.ShowBalloonTip(&balloon);
    return;
  }

  if (entry_->name_.empty()) {
    entry_->name_ = GenerateGUID();
    if (entry_->name_.empty()) {
      message.LoadString(IDS_ERR_UNEXPECTED);
      MessageBox(message, nullptr, MB_ICONERROR);
      return;
    }
  }

  entry_->bind_ = CStringA(bind_);

  temp.resize(service_combo_.GetWindowTextLength());
  GetWindowTextA(service_combo_, &temp[0], temp.size() + 1);
  entry_->service_name_ = temp;

  entry_->listen_ = listen_;
  entry_->type_ = type_combo_.GetCurSel() + 1;
  entry_->enabled_ = enabled_;

  EndDialog(IDOK);
}

void ServerDialog::OnCancel(UINT notify_code, int id, CWindow control) {
  EndDialog(IDCANCEL);
}
