// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_HTTP_HTTP_PROXY_H_
#define JUNO_SERVICE_HTTP_HTTP_PROXY_H_

#pragma warning(push, 3)
#pragma warning(disable : 4244)
#include <base/synchronization/condition_variable.h>
#include <base/synchronization/lock.h>
#pragma warning(pop)

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "service/service.h"

class HttpHeaders;
class HttpProxyConfig;
class HttpProxySession;
class HttpRequest;
class HttpResponse;

class HttpProxy : public Service {
 public:
  HttpProxy();
  ~HttpProxy();

  bool UpdateConfig(const ServiceConfigPtr& config) override;
  void Stop() override;

  void EndSession(HttpProxySession* session);

  void OnAccepted(const ChannelPtr& client) override;
  void OnReceivedFrom(const DatagramPtr& datagram) override;
  void OnError(HRESULT /*result*/) override {}

 private:
  typedef std::pair<HttpProxy*, HttpProxySession*> ServiceSessionPair;

  static void CALLBACK EndSessionImpl(PTP_CALLBACK_INSTANCE instance,
                                      void* param);
  void EndSessionImpl(HttpProxySession* session);

  std::shared_ptr<HttpProxyConfig> config_;

  base::Lock lock_;
  base::ConditionVariable empty_;
  bool stopped_;
  std::vector<std::unique_ptr<HttpProxySession>> sessions_;

  HttpProxy(const HttpProxy&) = delete;
  HttpProxy& operator=(const HttpProxy&) = delete;
};

#endif  // JUNO_SERVICE_HTTP_HTTP_PROXY_H_
