// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_SCISSORS_SCISSORS_H_
#define JUNO_NET_SCISSORS_SCISSORS_H_

#include <madoka/net/address_info.h>

#include <string>

#include "net/service_provider.h"

class SchannelCredential;

class Scissors : public ServiceProvider {
 public:
  Scissors();
  virtual ~Scissors();

  bool Setup(HKEY key);
  void Stop();

  bool OnAccepted(AsyncSocket* client);
  void OnError(DWORD error);

 private:
  friend class ScissorsSession;

  std::string remote_address_;
  int remote_port_;
  int remote_ssl_;

  madoka::net::AddressInfo resolver_;
  SchannelCredential* credential_;
};

#endif  // JUNO_NET_SCISSORS_SCISSORS_H_
