// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_SERVER_CONFIG_H_
#define JUNO_SERVICE_SERVER_CONFIG_H_

#include <string>

namespace juno {
namespace service {

struct ServerConfig {
  enum class Protocol {
    kTCP = 1,
    kUDP,
    kTLS,
  };

  std::wstring id_;
  std::string bind_;
  int listen_;
  int type_;
  std::wstring service_;
  int enabled_;
  std::string cert_hash_;
};

}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SERVER_CONFIG_H_
