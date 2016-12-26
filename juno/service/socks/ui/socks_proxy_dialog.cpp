// Copyright (c) 2013 dacci.org

#include "service/socks/ui/socks_proxy_dialog.h"

namespace juno {
namespace service {
namespace socks {
namespace ui {

SocksProxyDialog::SocksProxyDialog(ServiceConfig* config) : config_(config) {}

BOOL SocksProxyDialog::OnInitDialog(CWindow /*focus*/, LPARAM /*init_param*/) {
  DoDataExchange();
  return TRUE;
}

void SocksProxyDialog::OnOk(UINT /*notify_code*/, int /*id*/,
                            CWindow /*control*/) {
  EndDialog(IDOK);
}

void SocksProxyDialog::OnCancel(UINT /*notify_code*/, int /*id*/,
                                CWindow /*control*/) {
  EndDialog(IDCANCEL);
}

}  // namespace ui
}  // namespace socks
}  // namespace service
}  // namespace juno
