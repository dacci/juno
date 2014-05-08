// Copyright (c) 2014 dacci.org

#ifndef JUNO_APP_SERVER_CONFIG_H_
#define JUNO_APP_SERVER_CONFIG_H_

#include <memory>
#include <string>

class ServerConfig {
 public:
  ServerConfig() : listen_(), type_(), enabled_(1) {
  }

  ServerConfig(const ServerConfig& other)
      : name_(other.name_),
        bind_(other.bind_),
        listen_(other.listen_),
        type_(other.type_),
        service_name_(other.service_name_),
        enabled_(other.enabled_) {
  }

  std::string name_;
  std::string bind_;
  int listen_;
  int type_;
  std::string service_name_;
  int enabled_;

 private:
  ServerConfig& operator=(const ServerConfig&);
};

typedef std::shared_ptr<ServerConfig> ServerConfigPtr;

#endif  // JUNO_APP_SERVER_CONFIG_H_
