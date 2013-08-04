// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_SOCKS_SOCKS_PROXY_H_
#define JUNO_NET_SOCKS_SOCKS_PROXY_H_

#include <list>

#include "net/service_provider.h"

class SocksProxy : public ServiceProvider {
 public:
  SocksProxy();
  ~SocksProxy();

  bool Setup(HKEY key);
  void Stop();

  void OnAccepted(AsyncServerSocket* server, AsyncSocket* client, DWORD error);

 private:
  HANDLE empty_event_;
  CRITICAL_SECTION critical_section_;
};

#endif  // JUNO_NET_SOCKS_SOCKS_PROXY_H_
