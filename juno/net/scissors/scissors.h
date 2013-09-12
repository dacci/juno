// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_SCISSORS_SCISSORS_H_
#define JUNO_NET_SCISSORS_SCISSORS_H_

#include <madoka/concurrent/critical_section.h>
#include <madoka/net/address_info.h>

#include <list>
#include <string>

#include "net/service_provider.h"

class SchannelCredential;
class ScissorsSession;

class Scissors : public ServiceProvider {
 public:
  Scissors();
  virtual ~Scissors();

  bool Setup(HKEY key);
  void Stop();
  void EndSession(ScissorsSession* session);

  bool OnAccepted(AsyncSocket* client);
  bool OnReceivedFrom(Datagram* datagram);
  void OnError(DWORD error);

 private:
  friend class ScissorsSession;

  // TODO(dacci): replace with condition variable.
  HANDLE empty_event_;
  madoka::concurrent::CriticalSection critical_section_;
  std::list<ScissorsSession*> sessions_;

  bool stopped_;

  std::string remote_address_;
  int remote_port_;
  int remote_ssl_;

  madoka::net::AddressInfo resolver_;
  SchannelCredential* credential_;
};

#endif  // JUNO_NET_SCISSORS_SCISSORS_H_
