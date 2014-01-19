// Copyright (c) 2014 dacci.org

#ifndef JUNO_APP_SERVER_CONFIG_H_
#define JUNO_APP_SERVER_CONFIG_H_

#include <string>

class Server;

class ServerConfig {
 public:
  ServerConfig() : listen_(), type_(), enabled_(1), server_() {
  }

  ServerConfig(const ServerConfig& other)
      : name_(other.name_),
        bind_(other.bind_),
        listen_(other.listen_),
        type_(other.type_),
        service_name_(other.service_name_),
        enabled_(other.enabled_),
        server_() {
  }

  std::string name_;
  std::string bind_;
  int listen_;
  int type_;
  std::string service_name_;
  int enabled_;

  Server* server_;

 private:
  ServerConfig& operator=(const ServerConfig&);
};

#endif  // JUNO_APP_SERVER_CONFIG_H_
