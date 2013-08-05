// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_HTTP_HTTP_PROXY_H_
#define JUNO_NET_HTTP_HTTP_PROXY_H_

#include <list>

#include "net/service_provider.h"

class HttpProxySession;

class HttpProxy : public ServiceProvider {
 public:
  HttpProxy();
  ~HttpProxy();

  bool Setup(HKEY key);
  void Stop();
  void EndSession(HttpProxySession* session);

  bool OnAccepted(AsyncSocket* client);
  void OnError(DWORD error);

 private:
  HANDLE empty_event_;
  CRITICAL_SECTION critical_section_;
  std::list<HttpProxySession*> sessions_;
};

#endif  // JUNO_NET_HTTP_HTTP_PROXY_H_
