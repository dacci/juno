// Copyright (c) 2014 dacci.org

#ifndef JUNO_APP_SERVER_CONFIG_H_
#define JUNO_APP_SERVER_CONFIG_H_

#include <string>

class Server;

class ServerConfig {
 public:
  ServerConfig() : listen_(), type_(), enabled_(1) {
  }

  std::string name_;
  std::string bind_;
  int listen_;
  int type_;
  std::string service_name_;
  int enabled_;

  Server* server_;
};

#endif  // JUNO_APP_SERVER_CONFIG_H_
