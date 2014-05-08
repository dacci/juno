// Copyright (c) 2014 dacci.org

#ifndef JUNO_APP_SERVICE_CONFIG_H_
#define JUNO_APP_SERVICE_CONFIG_H_

#include <memory>
#include <string>

class ServiceConfig {
 public:
  // ServiceConfig(const ServiceConfig& other) = default;
  virtual ~ServiceConfig() {}

  std::string name_;
  std::string provider_name_;
};

typedef std::shared_ptr<ServiceConfig> ServiceConfigPtr;

#endif  // JUNO_APP_SERVICE_CONFIG_H_
