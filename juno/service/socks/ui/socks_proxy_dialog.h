// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SOCKS_UI_SOCKS_PROXY_DIALOG_H_
#define JUNO_SERVICE_SOCKS_UI_SOCKS_PROXY_DIALOG_H_

#include <atlbase.h>
#include <atlstr.h>
#include <atlwin.h>

#include <atlapp.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include <atlddx.h>
#include <atldlgs.h>

#include "res/resource.h"

namespace juno {
namespace service {

struct ServiceConfig;

namespace socks {
namespace ui {

class SocksProxyDialog : public CDialogImpl<SocksProxyDialog>,
                         public CWinDataExchange<SocksProxyDialog> {
 public:
  static const UINT IDD = IDD_SOCKS_PROXY;

  explicit SocksProxyDialog(ServiceConfig* config);

  BEGIN_MSG_MAP(SocksProxyDialog)
    MSG_WM_INITDIALOG(OnInitDialog)

    COMMAND_ID_HANDLER_EX(IDOK, OnOk)
    COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
  END_MSG_MAP()

  BEGIN_DDX_MAP(SocksProxyDialog)
  END_DDX_MAP()

 private:
  BOOL OnInitDialog(CWindow focus, LPARAM init_param);

  void OnOk(UINT notify_code, int id, CWindow control);
  void OnCancel(UINT notify_code, int id, CWindow control);

  ServiceConfig* config_;

  SocksProxyDialog(const SocksProxyDialog&) = delete;
  SocksProxyDialog& operator=(const SocksProxyDialog&) = delete;
};

}  // namespace ui
}  // namespace socks
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SOCKS_UI_SOCKS_PROXY_DIALOG_H_
