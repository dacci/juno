// Copyright (c) 2013 dacci.org

#ifndef JUNO_UI_SERVERS_PAGE_H_
#define JUNO_UI_SERVERS_PAGE_H_

#include <atlbase.h>

#include <atlapp.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include <atlddx.h>
#include <atldlgs.h>

#include <vector>

#include "res/resource.h"

class ServersPage
    : public CPropertyPageImpl<ServersPage>,
      public CWinDataExchange<ServersPage> {
 public:
  static const UINT IDD = IDD_SERVERS_PAGE;

  ServersPage();
  ~ServersPage();

  void OnPageRelease();

  BEGIN_DDX_MAP(ServersPage)
    DDX_CONTROL_HANDLE(IDC_SERVER_LIST, server_list_)
    DDX_CONTROL_HANDLE(IDC_ADD_BUTTON, add_button_)
    DDX_CONTROL_HANDLE(IDC_EDIT_BUTTON, edit_button_)
    DDX_CONTROL_HANDLE(IDC_DELETE_BUTTON, delete_button_)
  END_DDX_MAP()

  BEGIN_MSG_MAP(ServersPage)
    MSG_WM_INITDIALOG(OnInitDialog)

    COMMAND_ID_HANDLER_EX(IDC_ADD_BUTTON, OnAddServer)
    COMMAND_ID_HANDLER_EX(IDC_EDIT_BUTTON, OnEditServer)
    COMMAND_ID_HANDLER_EX(IDC_DELETE_BUTTON, OnDeleteServer)

    NOTIFY_HANDLER_EX(IDC_SERVER_LIST, NM_CLICK, OnServerListClicked)

    CHAIN_MSG_MAP(CPropertyPageImpl)
  END_MSG_MAP()

 private:
  struct ServerEntry {
    CString name;
    DWORD enabled;
    CString bind;
    DWORD listen;
    CString service;
  };

  BOOL OnInitDialog(CWindow focus, LPARAM init_param);

  void OnAddServer(UINT notify_code, int id, CWindow control);
  void OnEditServer(UINT notify_code, int id, CWindow control);
  void OnDeleteServer(UINT notify_code, int id, CWindow control);

  LRESULT OnServerListClicked(LPNMHDR header);

  std::vector<ServerEntry> servers_;
  CListViewCtrl server_list_;
  CButton add_button_;
  CButton edit_button_;
  CButton delete_button_;
};

#endif  // JUNO_UI_SERVERS_PAGE_H_
