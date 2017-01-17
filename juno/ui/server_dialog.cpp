// Copyright (c) 2013 dacci.org

#include "ui/server_dialog.h"

#include <iphlpapi.h>

#include <base/logging.h>
#include <base/strings/sys_string_conversions.h>

#include <string>

#include "misc/certificate_store.h"
#include "ui/preference_dialog.h"

namespace juno {
namespace ui {

using ::juno::service::ServerConfig;

ServerDialog::ServerDialog(PreferenceDialog* parent, ServerConfig* config)
    : parent_(parent), config_(config), listen_(0), enabled_(TRUE) {}

void ServerDialog::FillBindCombo() {
  bind_combo_.Clear();
  bind_combo_.AddString(_T("*"));
  bind_combo_.AddString(_T("localhost"));

  ULONG size = 0;
  GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, nullptr, &size);

  auto addresses = static_cast<IP_ADAPTER_ADDRESSES*>(malloc(size));
  auto error = GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, addresses, &size);
  ATLASSERT(error == ERROR_SUCCESS);
  if (error != ERROR_SUCCESS) {
    if (addresses != nullptr)
      free(addresses);

    return;
  }

  for (auto pointer = addresses; pointer; pointer = pointer->Next) {
    for (auto address = pointer->FirstUnicastAddress; address;
         address = address->Next) {
      CString text;
      DWORD length = 48;
      error = WSAAddressToString(address->Address.lpSockaddr,
                                 address->Address.iSockaddrLength, nullptr,
                                 text.GetBuffer(length), &length);
      ATLASSERT(error == 0);
      if (error != 0)
        continue;

      text.ReleaseBuffer(length);
      bind_combo_.AddString(text);
    }
  }

  free(addresses);
}

void ServerDialog::FillServiceCombo() {
  service_combo_.Clear();

  for (auto& service : parent_->service_configs_) {
    auto config = service.second.get();
    auto index = service_combo_.AddString(config->name_.c_str());
    service_combo_.SetItemDataPtr(index, config);
  }
}

BOOL ServerDialog::OnInitDialog(CWindow /*focus*/, LPARAM /*init_param*/) {
  bind_ = config_->bind_.c_str();
  listen_ = config_->listen_;
  enabled_ = config_->enabled_;

  DoDataExchange();

  FillBindCombo();

  type_combo_.AddString(_T("TCP"));
  type_combo_.AddString(_T("UDP"));
  type_combo_.AddString(_T("SSL/TLS"));
  detail_button_.EnableWindow(FALSE);

  FillServiceCombo();

  bind_combo_.SetWindowText(base::SysNativeMBToWide(config_->bind_).c_str());
  listen_edit_.SetLimitText(5);
  listen_spin_.SetRange32(0, 65535);
  type_combo_.SetCurSel(config_->type_ - 1);

  switch (static_cast<ServerConfig::Protocol>(config_->type_)) {
    case ServerConfig::Protocol::kTCP:
    case ServerConfig::Protocol::kUDP:
      detail_button_.EnableWindow(FALSE);
      break;

    case ServerConfig::Protocol::kTLS:
      detail_button_.EnableWindow(TRUE);
      break;
  }

  service_combo_.SelectString(-1, parent_->GetServiceName(config_->service_));

  return TRUE;
}

void ServerDialog::OnTypeChange(UINT /*notify_code*/, int /*id*/,
                                CWindow /*control*/) {
  switch (static_cast<ServerConfig::Protocol>(type_combo_.GetCurSel() + 1)) {
    case ServerConfig::Protocol::kTCP:
    case ServerConfig::Protocol::kUDP:
      detail_button_.EnableWindow(FALSE);
      break;

    case ServerConfig::Protocol::kTLS:
      detail_button_.EnableWindow();
      break;
  }
}

void ServerDialog::OnDetailSetting(UINT /*notify_code*/, int /*id*/,
                                   CWindow /*control*/) {
  auto protocol = type_combo_.GetCurSel() + 1;
  switch (static_cast<ServerConfig::Protocol>(protocol)) {
    case ServerConfig::Protocol::kTLS: {
      misc::CertificateStore store(L"MY");
      auto cert = store.SelectCertificate(m_hWnd, nullptr, nullptr, 0);
      if (cert == nullptr)
        break;

      DWORD length = 20;
      config_->cert_hash_.resize(length);
      if (CertGetCertificateContextProperty(cert, CERT_HASH_PROP_ID,
                                            &config_->cert_hash_[0], &length))
        config_->cert_hash_.resize(length);
      else
        config_->cert_hash_.clear();

      CertFreeCertificateContext(cert);

      break;
    }

    default:
      DLOG(FATAL) << "Invalid protocol: " << protocol;
      break;
  }
}

void ServerDialog::OnOk(UINT /*notify_code*/, int /*id*/, CWindow /*control*/) {
  HideBalloonTip();

  DoDataExchange(DDX_SAVE);

  if (bind_.IsEmpty()) {
    ShowBalloonTip(bind_combo_, IDS_NOT_SPECIFIED);
    return;
  }

  if (listen_ <= 0 || 65536 <= listen_) {
    CString text;
    text.LoadString(IDS_INVALID_PORT);

    EDITBALLOONTIP balloon{sizeof(balloon), nullptr, text};
    listen_edit_.ShowBalloonTip(&balloon);
    return;
  }

  if (type_combo_.GetCurSel() == CB_ERR) {
    ShowBalloonTip(type_combo_, IDS_NOT_SPECIFIED);
    return;
  }

  auto service_index = service_combo_.GetCurSel();
  if (service_index == CB_ERR) {
    ShowBalloonTip(service_combo_, IDS_NOT_SPECIFIED);
    return;
  }

  config_->bind_ = base::SysWideToNativeMB(bind_.GetString());

  auto service = static_cast<service::ServiceConfig*>(
      service_combo_.GetItemDataPtr(service_index));
  config_->service_ = service->id_;

  config_->listen_ = listen_;
  config_->type_ = type_combo_.GetCurSel() + 1;
  config_->enabled_ = enabled_;

  EndDialog(IDOK);
}

void ServerDialog::OnCancel(UINT /*notify_code*/, int /*id*/,
                            CWindow /*control*/) {
  EndDialog(IDCANCEL);
}

}  // namespace ui
}  // namespace juno
