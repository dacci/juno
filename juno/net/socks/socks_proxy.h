// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_SOCKS_SOCKS_PROXY_H_
#define JUNO_NET_SOCKS_SOCKS_PROXY_H_

#include <madoka/concurrent/condition_variable.h>
#include <madoka/concurrent/critical_section.h>

#include <list>

#include "app/service.h"

class SocksProxyConfig;
class SocksProxySession;

class SocksProxy : public Service {
 public:
  explicit SocksProxy(SocksProxyConfig* config);
  ~SocksProxy();

  bool Setup(HKEY key);
  void Stop();
  void EndSession(SocksProxySession* session);

  bool OnAccepted(madoka::net::AsyncSocket* client);
  bool OnReceivedFrom(Datagram* datagram);
  void OnError(DWORD error);

 private:
  madoka::concurrent::ConditionVariable empty_;
  madoka::concurrent::CriticalSection critical_section_;
  std::list<SocksProxySession*> sessions_;
  bool stopped_;
};

#endif  // JUNO_NET_SOCKS_SOCKS_PROXY_H_
