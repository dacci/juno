// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SOCKS_SOCKS_PROXY_H_
#define JUNO_SERVICE_SOCKS_SOCKS_PROXY_H_

#include <madoka/concurrent/condition_variable.h>
#include <madoka/concurrent/critical_section.h>

#include <memory>
#include <utility>
#include <vector>

#include "service/service.h"

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

  bool OnAccepted(const ChannelPtr& client) override;
  bool OnReceivedFrom(Datagram* datagram) override;
  void OnError(DWORD error) override;

 private:
  typedef std::pair<SocksProxy*, SocksProxySession*> ServiceSessionPair;

  static void CALLBACK EndSessionImpl(PTP_CALLBACK_INSTANCE instance,
                                      void* param);
  void EndSessionImpl(SocksProxySession* session);

  madoka::concurrent::ConditionVariable empty_;
  madoka::concurrent::CriticalSection critical_section_;
  std::vector<std::unique_ptr<SocksProxySession>> sessions_;
  bool stopped_;

  // disallow copy and assign
  SocksProxy(const SocksProxy&);
  SocksProxy& operator=(const SocksProxy&);
};

#endif  // JUNO_SERVICE_SOCKS_SOCKS_PROXY_H_
