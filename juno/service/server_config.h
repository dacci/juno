// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_SERVER_CONFIG_H_
#define JUNO_SERVICE_SERVER_CONFIG_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

namespace juno {
namespace service {

class ServerConfig {
 public:
  enum Protocol {
    TCP = 1,
    UDP,
    TLS,
  };

  ServerConfig() : listen_(), type_(), enabled_(1) {}

  ServerConfig(const ServerConfig& other)
      : name_(other.name_),
        bind_(other.bind_),
        listen_(other.listen_),
        type_(other.type_),
        service_name_(other.service_name_),
        enabled_(other.enabled_),
        cert_hash_(other.cert_hash_) {}

  std::string name_;
  std::string bind_;
  int listen_;
  int type_;
  std::string service_name_;
  int enabled_;
  std::vector<uint8_t> cert_hash_;

 private:
  ServerConfig& operator=(const ServerConfig&) = delete;
};

typedef std::shared_ptr<ServerConfig> ServerConfigPtr;

}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SERVER_CONFIG_H_
