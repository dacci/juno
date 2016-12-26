// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SOCKS_SOCKS_PROXY_H_
#define JUNO_SERVICE_SOCKS_SOCKS_PROXY_H_

#include <base/synchronization/condition_variable.h>
#include <base/synchronization/lock.h>

#include <list>
#include <memory>
#include <utility>

#include "service/service.h"

namespace juno {
namespace service {
namespace socks {

class SocksProxy;
class SocksProxyConfig;

class __declspec(novtable) SocksSession {
 public:
  SocksSession(SocksProxy* proxy, const io::ChannelPtr& channel)
      : proxy_(proxy), client_(channel) {}
  virtual ~SocksSession() {}

  virtual HRESULT Start(std::unique_ptr<char[]>&& request, int length) = 0;
  virtual void Stop() = 0;

 protected:
  SocksProxy* const proxy_;
  io::ChannelPtr client_;

  SocksSession(const SocksSession&) = delete;
  SocksSession& operator=(const SocksSession&) = delete;
};

class SocksProxy : public Service, private io::Channel::Listener {
 public:
  SocksProxy();
  ~SocksProxy();

  bool UpdateConfig(const ServiceConfigPtr& /*config*/) override {
    return true;
  }

  void Stop() override;
  void EndSession(SocksSession* session);

  void OnAccepted(const io::ChannelPtr& client) override;
  void OnReceivedFrom(const io::net::DatagramPtr& /*datagram*/) override {}
  void OnError(HRESULT /*result*/) override {}

 private:
  typedef std::pair<SocksProxy*, SocksSession*> ServiceSessionPair;

  static const int kBufferSize = 1024;

  void CheckEmpty();
  void WaitEmpty();

  void OnRead(io::Channel* channel, HRESULT result, void* buffer,
              int length) override;
  void OnWritten(io::Channel* channel, HRESULT result, void* buffer,
                 int length) override;

  static void CALLBACK EndSessionImpl(PTP_CALLBACK_INSTANCE instance,
                                      void* param);
  void EndSessionImpl(SocksSession* session);

  base::Lock lock_;
  base::ConditionVariable empty_;
  std::list<io::ChannelPtr> candidates_;
  std::list<std::unique_ptr<SocksSession>> sessions_;
  bool stopped_;

  SocksProxy(const SocksProxy&) = delete;
  SocksProxy& operator=(const SocksProxy&) = delete;
};

}  // namespace socks
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SOCKS_SOCKS_PROXY_H_
