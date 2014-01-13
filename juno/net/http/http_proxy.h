// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_HTTP_HTTP_PROXY_H_
#define JUNO_NET_HTTP_HTTP_PROXY_H_

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
  void Stop();

  void FilterHeaders(HttpHeaders* headers, bool request);
  void EndSession(HttpProxySession* session);

  bool OnAccepted(AsyncSocket* client);
  bool OnReceivedFrom(Datagram* datagram);
  void OnError(DWORD error);

 private:
  friend class HttpProxyProvider;

  HttpProxyConfig* const config_;

  // TODO(dacci): replace with condition variable
  HANDLE empty_event_;
  madoka::concurrent::CriticalSection critical_section_;
  bool stopped_;
  std::list<HttpProxySession*> sessions_;
};

#endif  // JUNO_NET_HTTP_HTTP_PROXY_H_
