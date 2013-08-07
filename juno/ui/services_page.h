// Copyright (c) 2013 dacci.org

#ifndef JUNO_UI_SERVICES_PAGE_H_
#define JUNO_UI_SERVICES_PAGE_H_

#include <atlbase.h>

#include <atlapp.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include <atlddx.h>
#include <atldlgs.h>

#include <vector>

#include "res/resource.h"

class ServicesPage
    : public CPropertyPageImpl<ServicesPage>,
      public CWinDataExchange<ServicesPage> {
 public:
  static const UINT IDD = IDD_SERVICES_PAGE;

  ServicesPage();
  ~ServicesPage();

  void OnPageRelease();

  BEGIN_DDX_MAP(ServicesPage)
    DDX_CONTROL_HANDLE(IDC_SERVICE_LIST, service_list_)
    DDX_CONTROL_HANDLE(IDC_ADD_BUTTON, add_button_)
    DDX_CONTROL_HANDLE(IDC_EDIT_BUTTON, edit_button_)
    DDX_CONTROL_HANDLE(IDC_DELETE_BUTTON, delete_button_)
  END_DDX_MAP()

  BEGIN_MSG_MAP(ServicesPage)
    MSG_WM_INITDIALOG(OnInitDialog)

    COMMAND_ID_HANDLER_EX(IDC_ADD_BUTTON, OnAddServer)
    COMMAND_ID_HANDLER_EX(IDC_EDIT_BUTTON, OnEditServer)
    COMMAND_ID_HANDLER_EX(IDC_DELETE_BUTTON, OnDeleteServer)

    NOTIFY_HANDLER_EX(IDC_SERVICE_LIST, NM_CLICK, OnServiceListClicked)

    CHAIN_MSG_MAP(CPropertyPageImpl)
  END_MSG_MAP()

 private:
  struct ServiceEntry {
    CString name;
    CString provider;
  };

  BOOL OnInitDialog(CWindow focus, LPARAM init_param);

  void OnAddServer(UINT notify_code, int id, CWindow control);
  void OnEditServer(UINT notify_code, int id, CWindow control);
  void OnDeleteServer(UINT notify_code, int id, CWindow control);

  LRESULT OnServiceListClicked(LPNMHDR header);

  std::vector<ServiceEntry> services_;
  CListViewCtrl service_list_;
  CButton add_button_;
  CButton edit_button_;
  CButton delete_button_;
};

#endif  // JUNO_UI_SERVICES_PAGE_H_
