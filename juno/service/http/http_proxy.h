// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_HTTP_HTTP_PROXY_H_
#define JUNO_SERVICE_HTTP_HTTP_PROXY_H_

#include <base/synchronization/condition_variable.h>
#include <base/synchronization/lock.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "service/service.h"
#include "service/http/http_digest.h"
#include "service/http/http_proxy_config.h"

namespace juno {
namespace service {
namespace http {

class HttpHeaders;
class HttpProxySession;
class HttpRequest;
class HttpResponse;

class HttpProxy : public Service {
 public:
  HttpProxy();
  ~HttpProxy();

  bool UpdateConfig(const ServiceConfig* config) override;
  void Stop() override;

  void EndSession(HttpProxySession* session);

  void FilterHeaders(HttpHeaders* headers, bool request) const;
  void ProcessAuthenticate(HttpResponse* response, HttpRequest* request);
  void ProcessAuthorization(HttpRequest* request);

  void OnAccepted(std::unique_ptr<io::Channel>&& client) override;
  void OnReceivedFrom(std::unique_ptr<io::net::Datagram>&& datagram) override;

 private:
  typedef std::pair<HttpProxy*, HttpProxySession*> ServiceSessionPair;

  static void CALLBACK EndSessionImpl(PTP_CALLBACK_INSTANCE instance,
                                      void* param);
  void EndSessionImpl(HttpProxySession* session);

  void SetCredential();
  void DoProcessAuthenticate(HttpResponse* response);
  void DoProcessAuthorization(HttpRequest* request);

  const HttpProxyConfig* config_;

  base::Lock lock_;
  base::ConditionVariable empty_;
  bool stopped_;
  std::vector<std::unique_ptr<HttpProxySession>> sessions_;

  bool auth_digest_;
  bool auth_basic_;
  HttpDigest digest_;
  std::string basic_credential_;

  HttpProxy(const HttpProxy&) = delete;
  HttpProxy& operator=(const HttpProxy&) = delete;
};

}  // namespace http
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_HTTP_HTTP_PROXY_H_
