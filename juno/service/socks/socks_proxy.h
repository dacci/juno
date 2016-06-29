// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SOCKS_SOCKS_PROXY_H_
#define JUNO_SERVICE_SOCKS_SOCKS_PROXY_H_

#pragma warning(push, 3)
#pragma warning(disable : 4244)
#include <base/synchronization/condition_variable.h>
#include <base/synchronization/lock.h>
#pragma warning(pop)

#include <memory>
#include <utility>
#include <vector>

#include "service/service.h"

class SocksProxyConfig;
class SocksProxySession;

class SocksProxy : public Service {
 public:
  SocksProxy();
  ~SocksProxy();

  bool UpdateConfig(const ServiceConfigPtr& /*config*/) override {
    return true;
  }

  void Stop() override;
  void EndSession(SocksProxySession* session);

  void OnAccepted(const ChannelPtr& client) override;
  void OnReceivedFrom(const DatagramPtr& /*datagram*/) override {}
  void OnError(HRESULT /*result*/) override {}

 private:
  typedef std::pair<SocksProxy*, SocksProxySession*> ServiceSessionPair;

  static void CALLBACK EndSessionImpl(PTP_CALLBACK_INSTANCE instance,
                                      void* param);
  void EndSessionImpl(SocksProxySession* session);

  base::Lock lock_;
  base::ConditionVariable empty_;
  std::vector<std::unique_ptr<SocksProxySession>> sessions_;
  bool stopped_;

  SocksProxy(const SocksProxy&) = delete;
  SocksProxy& operator=(const SocksProxy&) = delete;
};

#endif  // JUNO_SERVICE_SOCKS_SOCKS_PROXY_H_
