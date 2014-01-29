// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_SCISSORS_SCISSORS_H_
#define JUNO_NET_SCISSORS_SCISSORS_H_

#include <madoka/concurrent/condition_variable.h>
#include <madoka/concurrent/critical_section.h>

#include <list>
#include <map>
#include <string>

#include "app/service.h"
#include "net/async_datagram_socket.h"

class SchannelCredential;
class ScissorsConfig;

class Scissors : public Service {
 public:
  class Session {
   public:
    virtual ~Session() {}

    virtual void Stop() = 0;
  };

  explicit Scissors(ScissorsConfig* config);
  virtual ~Scissors();

  bool Init();
  void Stop();

  bool OnAccepted(AsyncSocket* client);
  bool OnReceivedFrom(Datagram* datagram);
  void OnError(DWORD error);

 private:
  friend class ScissorsProvider;
  friend class ScissorsTcpSession;
  friend class ScissorsUdpSession;

  static const int kBufferSize = 8192;

  void EndSession(Session* session);

  ScissorsConfig* const config_;

  madoka::concurrent::ConditionVariable empty_;
  madoka::concurrent::CriticalSection critical_section_;
  std::list<Session*> sessions_;

  std::map<AsyncDatagramSocket*, Datagram*> datagrams_;
  std::map<AsyncDatagramSocket*, char*> buffers_;

  bool stopped_;

  SchannelCredential* credential_;
};

#endif  // JUNO_NET_SCISSORS_SCISSORS_H_
