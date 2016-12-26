// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_CONFIG_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_CONFIG_H_

#include <string>

#include "misc/registry_key.h"
#include "service/service_config.h"

namespace juno {
namespace service {
namespace scissors {
namespace ui {

class ScissorsDialog;

}  // namespace ui

class ScissorsConfig : public ServiceConfig {
 public:
  ScissorsConfig();
  ScissorsConfig(const ScissorsConfig& other);

  static std::shared_ptr<ScissorsConfig> Load(const misc::RegistryKey& key);
  bool Save(misc::RegistryKey* key) const;

  std::string remote_address() const {
    return remote_address_;
  }

  int remote_port() const {
    return remote_port_;
  }

  bool remote_ssl() const {
    return remote_ssl_;
  }

  bool remote_udp() const {
    return remote_udp_;
  }

 private:
  friend class ui::ScissorsDialog;

  std::string remote_address_;
  int remote_port_;
  bool remote_ssl_;
  bool remote_udp_;

  ScissorsConfig& operator=(const ScissorsConfig&) = delete;
};

}  // namespace scissors
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_CONFIG_H_
