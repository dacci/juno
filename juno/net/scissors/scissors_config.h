// Copyright (c) 2014 dacci.org

#ifndef JUNO_NET_SCISSORS_SCISSORS_CONFIG_H_
#define JUNO_NET_SCISSORS_SCISSORS_CONFIG_H_

#include <madoka/concurrent/read_write_lock.h>

#include "app/service_config.h"
#include "misc/registry_key.h"

class ScissorsConfig : public ServiceConfig {
 public:
  ScissorsConfig();
  ScissorsConfig(const ScissorsConfig& other);

  virtual ~ScissorsConfig();

  ScissorsConfig& operator=(const ScissorsConfig& other);

  bool Load(const RegistryKey& key);
  bool Save(RegistryKey* key);

  std::string remote_address();
  void set_remote_address(const std::string& remote_address);

  int remote_port();
  void set_remote_port(int remote_port);

  int remote_ssl();
  void set_remote_ssl(int remote_ssl);

  int remote_udp();
  void set_remote_udp(int remote_udp);

 private:
  madoka::concurrent::ReadWriteLock lock_;

  std::string remote_address_;
  int remote_port_;
  int remote_ssl_;
  int remote_udp_;
};

#endif  // JUNO_NET_SCISSORS_SCISSORS_CONFIG_H_
