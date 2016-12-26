// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_SERVICE_CONFIG_H_
#define JUNO_SERVICE_SERVICE_CONFIG_H_

#include <memory>
#include <string>

namespace juno {
namespace service {

class ServiceConfig {
 public:
  // ServiceConfig(const ServiceConfig& other) = default;
  virtual ~ServiceConfig() {}

  std::string name_;
  std::string provider_name_;
};

typedef std::shared_ptr<ServiceConfig> ServiceConfigPtr;

}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SERVICE_CONFIG_H_
