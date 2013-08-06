// Copyright (c) 2013 dacci.org

#include <crtdbg.h>
#include <stdint.h>

#include <atlbase.h>
#include <atlstr.h>

#include <madoka/net/winsock.h>

#include "app/service_manager.h"

int main(int argc, char* argv[]) {
#ifdef _DEBUG
  {
    int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    flags |= _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF;
    _CrtSetDbgFlag(flags);
  }
#endif  // _DEBUG

  madoka::net::WinSock winsock(WINSOCK_VERSION);
  if (!winsock.Initialized())
    return __LINE__;

  ServiceManager service_manager;
  if (!service_manager.LoadServices())
    return __LINE__;
  if (!service_manager.LoadServers())
    return __LINE__;
  if (!service_manager.StartServers())
    return __LINE__;

  ::getchar();

  service_manager.UnloadServers();
  service_manager.UnloadServices();

  return 0;
}
