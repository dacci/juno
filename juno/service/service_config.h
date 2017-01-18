// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_SERVICE_CONFIG_H_
#define JUNO_SERVICE_SERVICE_CONFIG_H_

#include <string>

namespace juno {
namespace service {

struct ServiceConfig {
  virtual ~ServiceConfig() {}

  std::wstring id_;
  std::wstring name_;
  std::wstring provider_;
};

}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SERVICE_CONFIG_H_
