// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_HTTP_HTTP_PROXY_H_
#define JUNO_SERVICE_HTTP_HTTP_PROXY_H_

#include <madoka/concurrent/condition_variable.h>
#include <madoka/concurrent/read_write_lock.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/service_manager.h"
#include "service/service.h"
#include "service/http/http_digest.h"

class HttpHeaders;
class HttpProxyConfig;
class HttpProxySession;
class HttpRequest;
class HttpResponse;

class HttpProxy : public Service {
 public:
  explicit HttpProxy(const ServiceConfigPtr& config);
  ~HttpProxy();

  bool Init();
  bool UpdateConfig(const ServiceConfigPtr& config) override;
  void Stop() override;

  void FilterHeaders(HttpHeaders* headers, bool request);
  void ProcessAuthenticate(HttpResponse* response, HttpRequest* request);
  void ProcessAuthorization(HttpRequest* request);

  bool use_remote_proxy();
  const char* remote_proxy_host();
  int remote_proxy_port();
  int auth_remote_proxy();

  void EndSession(HttpProxySession* session);

  void OnAccepted(const ChannelPtr& client) override;
  void OnReceivedFrom(const DatagramPtr& datagram) override;
  void OnError(DWORD error) override;

 private:
  typedef std::pair<HttpProxy*, HttpProxySession*> ServiceSessionPair;

  static const std::string kProxyAuthenticate;
  static const std::string kProxyAuthorization;

  void DoProcessAuthenticate(HttpResponse* response);
  void DoProcessAuthorization(HttpRequest* request);
  void SetBasicCredential();

  static void CALLBACK EndSessionImpl(PTP_CALLBACK_INSTANCE instance,
                                      void* param);
  void EndSessionImpl(HttpProxySession* session);

  std::shared_ptr<HttpProxyConfig> config_;

  madoka::concurrent::ConditionVariable empty_;
  madoka::concurrent::ReadWriteLock lock_;
  bool stopped_;
  std::vector<std::unique_ptr<HttpProxySession>> sessions_;
  bool auth_digest_;
  bool auth_basic_;
  HttpDigest digest_;
  std::string basic_credential_;
};

#endif  // JUNO_SERVICE_HTTP_HTTP_PROXY_H_
