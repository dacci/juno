// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_SOCKS_SOCKS_PROXY_H_
#define JUNO_NET_SOCKS_SOCKS_PROXY_H_

#include <madoka/concurrent/critical_section.h>

#include <list>

#include "net/service_provider.h"

class SocksProxySession;

class SocksProxy : public ServiceProvider {
 public:
  SocksProxy();
  ~SocksProxy();

  bool Setup(HKEY key);
  void Stop();
  void EndSession(SocksProxySession* session);

  bool OnAccepted(AsyncSocket* client);
  bool OnReceivedFrom(Datagram* datagram);
  void OnError(DWORD error);

 private:
  // TODO(dacci): replace with condition variable.
  HANDLE empty_event_;
  madoka::concurrent::CriticalSection critical_section_;
  std::list<SocksProxySession*> sessions_;
  bool stopped_;
};

#endif  // JUNO_NET_SOCKS_SOCKS_PROXY_H_
