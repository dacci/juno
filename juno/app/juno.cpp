// Copyright (c) 2013 dacci.org

#include <crtdbg.h>
#include <stdint.h>

#include <atlbase.h>
#include <atlutil.h>

#include <madoka/net/winsock.h>

#include "net/http/http_proxy.h"

int main(int argc, char* argv[]) {
#ifdef _DEBUG
  {
    int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    flags |= _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF;
    _CrtSetDbgFlag(flags);
  }
#endif  // _DEBUG

  const char* port = "8080";
  if (argc >= 2)
    port = argv[1];

  const char* address = "127.0.0.1";
  if (argc >= 3)
    address = argv[2];

  madoka::net::WinSock winsock(WINSOCK_VERSION);
  if (!winsock.Initialized())
    return winsock.error();

  HttpProxy proxy(address, port);
  if (!proxy.Start())
    return __LINE__;

  ::getchar();

  proxy.Stop();

  return 0;
}
