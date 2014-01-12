// Copyright (c) 2014 dacci.org

#ifndef JUNO_APP_SERVICE_CONFIG_H_
#define JUNO_APP_SERVICE_CONFIG_H_

#include <string>

class ServiceProvider;

class ServiceConfig {
 public:
  virtual ~ServiceConfig() {}

  std::string name_;
  std::string provider_name_;
  ServiceProvider* provider_;
};

#endif  // JUNO_APP_SERVICE_CONFIG_H_
