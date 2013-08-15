// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_HTTP_HTTP_PROXY_H_
#define JUNO_NET_HTTP_HTTP_PROXY_H_

#include <list>
#include <string>
#include <vector>

#include "net/service_provider.h"

class HttpHeaders;
class HttpProxySession;

class HttpProxy : public ServiceProvider {
 public:
  HttpProxy();
  ~HttpProxy();

  bool Setup(HKEY key);
  void Stop();

  void FilterHeaders(HttpHeaders* headers, bool request);
  void EndSession(HttpProxySession* session);

  bool OnAccepted(AsyncSocket* client);
  void OnError(DWORD error);

  bool use_remote_proxy() const {
    return use_remote_proxy_ != 0;
  }

  const char* remote_proxy_host() const {
    return remote_proxy_host_.c_str();
  }

  int remote_proxy_port() const {
    return remote_proxy_port_;
  }

  DWORD auth_remote_proxy() const {
    return auth_remote_proxy_;
  }

  const std::string& remote_proxy_auth() const {
    return remote_proxy_auth_;
  }

 private:
  enum FilterAction {
    Add,
    Set,
    Append,
    Unset,
    Merge,
    Echo,
    Replace,
    ReplaceAll,
  };

  struct HeaderFilter {
    bool request;
    bool response;
    FilterAction action;
    std::string name;
    std::string value;
    std::string replace;
  };

  HANDLE empty_event_;
  CRITICAL_SECTION critical_section_;
  bool stopped_;
  std::list<HttpProxySession*> sessions_;

  DWORD use_remote_proxy_;
  std::string remote_proxy_host_;
  DWORD remote_proxy_port_;
  DWORD auth_remote_proxy_;
  std::string remote_proxy_auth_;
  std::vector<HeaderFilter> header_filters_;
};

#endif  // JUNO_NET_HTTP_HTTP_PROXY_H_
