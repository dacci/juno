// Copyright (c) 2013 dacci.org

#ifndef JUNO_UI_SERVERS_PAGE_H_
#define JUNO_UI_SERVERS_PAGE_H_

#include <atlbase.h>
#include <atlwin.h>

#include <atlapp.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include <atldlgs.h>

#include "res/resource.h"
#include "service/server_config.h"
#include "ui/data_exchange_ex.h"

namespace juno {
namespace ui {

class PreferenceDialog;

class ServersPage : public CPropertyPageImpl<ServersPage>,
                    public DataExchangeEx<ServersPage> {
 public:
  static const UINT IDD = IDD_SERVERS_PAGE;

  explicit ServersPage(PreferenceDialog* parent);

  BEGIN_MSG_MAP(ServersPage)
    MSG_WM_INITDIALOG(OnInitDialog)

    COMMAND_ID_HANDLER_EX(IDC_ADD_BUTTON, OnAddServer)
    COMMAND_ID_HANDLER_EX(IDC_EDIT_BUTTON, OnEditServer)
    COMMAND_ID_HANDLER_EX(IDC_DELETE_BUTTON, OnDeleteServer)

    NOTIFY_HANDLER_EX(IDC_SERVER_LIST, LVN_ITEMCHANGED, OnServerListChanged)
    NOTIFY_HANDLER_EX(IDC_SERVER_LIST, NM_DBLCLK, OnServerListDoubleClicked)

    CHAIN_MSG_MAP(CPropertyPageImpl)
  END_MSG_MAP()

  BEGIN_DDX_MAP(ServersPage)
    DDX_CONTROL_HANDLE(IDC_SERVER_LIST, server_list_)
    DDX_CONTROL_HANDLE(IDC_ADD_BUTTON, add_button_)
    DDX_CONTROL_HANDLE(IDC_EDIT_BUTTON, edit_button_)
    DDX_CONTROL_HANDLE(IDC_DELETE_BUTTON, delete_button_)
  END_DDX_MAP()

 private:
  void AddServerItem(const service::ServerConfig* config, int index);

  BOOL OnInitDialog(CWindow focus, LPARAM init_param);

  void OnAddServer(UINT notify_code, int id, CWindow control);
  void OnEditServer(UINT notify_code, int id, CWindow control);
  void OnDeleteServer(UINT notify_code, int id, CWindow control);

  LRESULT OnServerListChanged(LPNMHDR header);
  LRESULT OnServerListDoubleClicked(LPNMHDR header);

  PreferenceDialog* const parent_;
  bool initialized_;

  CListViewCtrl server_list_;
  CButton add_button_;
  CButton edit_button_;
  CButton delete_button_;

  ServersPage(const ServersPage&) = delete;
  ServersPage& operator=(const ServersPage&) = delete;
};

}  // namespace ui
}  // namespace juno

#endif  // JUNO_UI_SERVERS_PAGE_H_
