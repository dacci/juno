// Copyright (c) 2013 dacci.org

#ifndef JUNO_UI_SERVERS_PAGE_H_
#define JUNO_UI_SERVERS_PAGE_H_

#include <atlbase.h>

#include <atlapp.h>
#include <atlcrack.h>
#include <atldlgs.h>

#include "res/resource.h"

class ServersPage : public CPropertyPageImpl<ServersPage> {
 public:
  static const UINT IDD = IDD_SERVERS_PAGE;

  ServersPage();
  ~ServersPage();

  void OnPageRelease();

  BEGIN_MSG_MAP(ServersPage)
    CHAIN_MSG_MAP(CPropertyPageImpl)
  END_MSG_MAP()
};

#endif  // JUNO_UI_SERVERS_PAGE_H_
