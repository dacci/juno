// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_HTTP_HTTP_PROXY_H_
#define JUNO_NET_HTTP_HTTP_PROXY_H_

#include <madoka/concurrent/condition_variable.h>
#include <madoka/concurrent/critical_section.h>

#include <list>
#include <string>
#include <vector>

#include "app/service.h"

class HttpHeaders;
class HttpProxyConfig;
class HttpProxySession;

class HttpProxy : public Service {
 public:
  explicit HttpProxy(HttpProxyConfig* config);
  ~HttpProxy();

  bool Init();
  void Stop() override;

  void FilterHeaders(HttpHeaders* headers, bool request);
  void EndSession(HttpProxySession* session);

  bool OnAccepted(madoka::net::AsyncSocket* client) override;
  bool OnReceivedFrom(Datagram* datagram) override;
  void OnError(DWORD error) override;

 private:
  friend class HttpProxyProvider;

  HttpProxyConfig* const config_;

  madoka::concurrent::ConditionVariable empty_;
  madoka::concurrent::CriticalSection critical_section_;
  bool stopped_;
  std::list<HttpProxySession*> sessions_;
};

#endif  // JUNO_NET_HTTP_HTTP_PROXY_H_
