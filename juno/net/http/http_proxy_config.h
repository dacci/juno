// Copyright (c) 2014 dacci.org

#ifndef JUNO_NET_HTTP_HTTP_PROXY_CONFIG_H_
#define JUNO_NET_HTTP_HTTP_PROXY_CONFIG_H_

#include <vector>

#include "app/service_config.h"
#include "misc/registry_key.h"

class HttpHeaders;

class HttpProxyConfig : public ServiceConfig {
 public:
  enum FilterAction {
    Set, Append, Add, Unset, Merge, Edit, EditR,
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

  virtual ~HttpProxyConfig();

  HttpProxyConfig& operator=(const HttpProxyConfig& other);

  bool Load(const RegistryKey& key);
  bool Save(RegistryKey* key);

  inline int use_remote_proxy() const {
    return use_remote_proxy_;
  }

  inline const std::string& remote_proxy_host() const {
    return remote_proxy_host_;
  }

  inline int remote_proxy_port() const {
    return remote_proxy_port_;
  }

  inline int auth_remote_proxy() const {
    return auth_remote_proxy_;
  }

  inline const std::string& remote_proxy_user() const {
    return remote_proxy_user_;
  }

  inline const std::string& remote_proxy_password() const {
    return remote_proxy_password_;
  }

  inline const std::vector<HeaderFilter>& header_filters() const {
    return header_filters_;
  }

 private:
  friend class HttpProxyDialog;

  int use_remote_proxy_;
  std::string remote_proxy_host_;
  int remote_proxy_port_;
  int auth_remote_proxy_;
  std::string remote_proxy_user_;
  std::string remote_proxy_password_;
  std::vector<HeaderFilter> header_filters_;
};

#endif  // JUNO_NET_HTTP_HTTP_PROXY_CONFIG_H_
