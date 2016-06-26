// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_HTTP_HTTP_PROXY_CONFIG_H_
#define JUNO_SERVICE_HTTP_HTTP_PROXY_CONFIG_H_

#pragma warning(push, 3)
#pragma warning(disable : 4244)
#include <base/synchronization/lock.h>
#pragma warning(pop)

#include <string>
#include <vector>

#include "misc/registry_key.h"
#include "service/service_config.h"
#include "service/http/http_digest.h"

class HttpHeaders;
class HttpRequest;
class HttpResponse;

class HttpProxyConfig : public ServiceConfig {
 public:
  enum FilterAction {
    Set,
    Append,
    Add,
    Unset,
    Merge,
    Edit,
    EditR,
  };

  struct HeaderFilter {
    bool request;
    bool response;
    FilterAction action;
    std::string name;
    std::string value;
    std::string replace;
  };

  HttpProxyConfig();
  HttpProxyConfig(const HttpProxyConfig& other);

  static std::shared_ptr<HttpProxyConfig> Load(const RegistryKey& key);
  bool Save(RegistryKey* key);

  void FilterHeaders(HttpHeaders* headers, bool request) const;
  void ProcessAuthenticate(HttpResponse* response, HttpRequest* request);
  void ProcessAuthorization(HttpRequest* request);

  bool use_remote_proxy() const {
    return use_remote_proxy_;
  }

  const std::string& remote_proxy_host() const {
    return remote_proxy_host_;
  }

  int remote_proxy_port() const {
    return remote_proxy_port_;
  }

  bool auth_remote_proxy() const {
    return auth_remote_proxy_;
  }

  const std::string& remote_proxy_user() const {
    return remote_proxy_user_;
  }

  const std::string& remote_proxy_password() const {
    return remote_proxy_password_;
  }

  const std::vector<HeaderFilter>& header_filters() const {
    return header_filters_;
  }

 private:
  friend class HttpProxyDialog;

  void SetCredential();

  void DoProcessAuthenticate(HttpResponse* response);
  void DoProcessAuthorization(HttpRequest* request);

  bool use_remote_proxy_;
  std::string remote_proxy_host_;
  int remote_proxy_port_;
  bool auth_remote_proxy_;
  std::string remote_proxy_user_;
  std::string remote_proxy_password_;
  std::vector<HeaderFilter> header_filters_;

  base::Lock lock_;
  bool auth_digest_;
  bool auth_basic_;
  HttpDigest digest_;
  std::string basic_credential_;

  HttpProxyConfig& operator=(const HttpProxyConfig& other) = delete;
};

#endif  // JUNO_SERVICE_HTTP_HTTP_PROXY_CONFIG_H_
