// Copyright (c) 2013 dacci.org

#ifndef JUNO_UI_SERVICES_PAGE_H_
#define JUNO_UI_SERVICES_PAGE_H_

#include <atlbase.h>

#include <atlapp.h>
#include <atlcrack.h>
#include <atldlgs.h>

#include "res/resource.h"

class ServicesPage : public CPropertyPageImpl<ServicesPage> {
 public:
  static const UINT IDD = IDD_SERVICES_PAGE;

  ServicesPage();
  ~ServicesPage();

  void OnPageRelease();

  BEGIN_MSG_MAP(ServicesPage)
    CHAIN_MSG_MAP(CPropertyPageImpl)
  END_MSG_MAP()
};

#endif  // JUNO_UI_SERVICES_PAGE_H_
