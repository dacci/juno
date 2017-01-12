// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_HTTP_HTTP_PROXY_CONFIG_H_
#define JUNO_SERVICE_HTTP_HTTP_PROXY_CONFIG_H_

#include <string>
#include <vector>

#include "service/service_config.h"

namespace juno {
namespace service {
namespace http {

struct HttpProxyConfig : ServiceConfig {
  enum class FilterAction {
    kSet,
    kAppend,
    kAdd,
    kUnset,
    kMerge,
    kEdit,
    kEditR,
  };

  struct HeaderFilter {
    bool request;
    bool response;
    FilterAction action;
    std::string name;
    std::string value;
    std::string replace;
  };

  bool use_remote_proxy_;
  std::string remote_proxy_host_;
  int remote_proxy_port_;
  bool auth_remote_proxy_;
  std::string remote_proxy_user_;
  std::string remote_proxy_password_;
  std::vector<HeaderFilter> header_filters_;
};

}  // namespace http
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_HTTP_HTTP_PROXY_CONFIG_H_
