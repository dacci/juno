// Copyright (c) 2013 dacci.org

#ifndef JUNO_UI_SERVICES_PAGE_H_
#define JUNO_UI_SERVICES_PAGE_H_

#include <atlbase.h>
#include <atlwin.h>

#include <atlapp.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include <atldlgs.h>

#include "res/resource.h"
#include "service/service_config.h"
#include "ui/data_exchange_ex.h"

namespace juno {
namespace ui {

class PreferenceDialog;

class ServicesPage : public CPropertyPageImpl<ServicesPage>,
                     public DataExchangeEx<ServicesPage> {
 public:
  static const UINT IDD = IDD_SERVICES_PAGE;

  explicit ServicesPage(PreferenceDialog* parent);

  BEGIN_MSG_MAP(ServicesPage)
    MSG_WM_INITDIALOG(OnInitDialog)

    COMMAND_ID_HANDLER_EX(IDC_ADD_BUTTON, OnAddService)
    COMMAND_ID_HANDLER_EX(IDC_EDIT_BUTTON, OnEditService)
    COMMAND_ID_HANDLER_EX(IDC_DELETE_BUTTON, OnDeleteService)

    NOTIFY_HANDLER_EX(IDC_SERVICE_LIST, LVN_ITEMCHANGED, OnServiceListChanged)
    NOTIFY_HANDLER_EX(IDC_SERVICE_LIST, NM_DBLCLK, OnServiceListDoubleClicked)

    CHAIN_MSG_MAP(CPropertyPageImpl)
  END_MSG_MAP()

  BEGIN_DDX_MAP(ServicesPage)
    DDX_CONTROL_HANDLE(IDC_SERVICE_LIST, service_list_)
    DDX_CONTROL_HANDLE(IDC_ADD_BUTTON, add_button_)
    DDX_CONTROL_HANDLE(IDC_EDIT_BUTTON, edit_button_)
    DDX_CONTROL_HANDLE(IDC_DELETE_BUTTON, delete_button_)
  END_DDX_MAP()

 private:
  void AddServiceItem(const service::ServiceConfig* config, int index);

  BOOL OnInitDialog(CWindow focus, LPARAM init_param);

  void OnAddService(UINT notify_code, int id, CWindow control);
  void OnEditService(UINT notify_code, int id, CWindow control);
  void OnDeleteService(UINT notify_code, int id, CWindow control);

  LRESULT OnServiceListChanged(LPNMHDR header);
  LRESULT OnServiceListDoubleClicked(LPNMHDR header);

  PreferenceDialog* const parent_;
  bool initialized_;

  CListViewCtrl service_list_;
  CButton add_button_;
  CButton edit_button_;
  CButton delete_button_;

  ServicesPage(const ServicesPage&) = delete;
  ServicesPage& operator=(const ServicesPage&) = delete;
};

}  // namespace ui
}  // namespace juno

#endif  // JUNO_UI_SERVICES_PAGE_H_
