// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_SERVER_CONFIG_H_
#define JUNO_SERVICE_SERVER_CONFIG_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

namespace juno {
namespace service {

struct ServerConfig {
  enum class Protocol {
    kTCP = 1,
    kUDP,
    kTLS,
  };

  std::string id_;
  std::string bind_;
  int listen_;
  int type_;
  std::string service_;
  int enabled_;
  std::vector<uint8_t> cert_hash_;
};

typedef std::shared_ptr<ServerConfig> ServerConfigPtr;

}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SERVER_CONFIG_H_
