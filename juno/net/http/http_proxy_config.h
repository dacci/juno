// Copyright (c) 2014 dacci.org

#ifndef JUNO_NET_HTTP_HTTP_PROXY_CONFIG_H_
#define JUNO_NET_HTTP_HTTP_PROXY_CONFIG_H_

#include <madoka/concurrent/read_write_lock.h>

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

  bool Load(const RegistryKey& key);

  int use_remote_proxy();
  std::string remote_proxy_host();
  int remote_proxy_port();
  int auth_remote_proxy();
  std::string remote_proxy_user();
  std::string remote_proxy_password();
  std::string basic_authorization();

 private:
  friend class HttpProxy;

  madoka::concurrent::ReadWriteLock lock_;

  int use_remote_proxy_;
  std::string remote_proxy_host_;
  int remote_proxy_port_;
  int auth_remote_proxy_;
  std::string remote_proxy_user_;
  std::string remote_proxy_password_;
  std::vector<HeaderFilter> header_filters_;

  std::string basic_authorization_;
};

#endif  // JUNO_NET_HTTP_HTTP_PROXY_CONFIG_H_
