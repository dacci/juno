// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_CONFIG_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_CONFIG_H_

#include <string>

#include "service/service_config.h"

namespace juno {
namespace service {
namespace scissors {

struct ScissorsConfig : ServiceConfig {
  std::string remote_address_;
  int remote_port_;
  bool remote_ssl_;
  bool remote_udp_;
};

}  // namespace scissors
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_CONFIG_H_
