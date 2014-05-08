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
  SocksProxy();
  virtual ~SocksProxy();

  bool Setup(HKEY key);
  bool UpdateConfig(const ServiceConfigPtr& config) override;
  void Stop() override;
  void EndSession(SocksProxySession* session);

  bool OnAccepted(madoka::net::AsyncSocket* client) override;
  bool OnReceivedFrom(Datagram* datagram) override;
  void OnError(DWORD error) override;

 private:
  madoka::concurrent::ConditionVariable empty_;
  madoka::concurrent::CriticalSection critical_section_;
  std::list<SocksProxySession*> sessions_;
  bool stopped_;

  // disallow copy and assign
  SocksProxy(const SocksProxy&);
  SocksProxy& operator=(const SocksProxy&);
};

#endif  // JUNO_NET_SOCKS_SOCKS_PROXY_H_
