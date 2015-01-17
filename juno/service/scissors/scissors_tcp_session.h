// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_TCP_SESSION_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_TCP_SESSION_H_

#include <madoka/net/socket_event_listener.h>

#include "service/scissors/scissors.h"

class ScissorsTcpSession
    : public Scissors::Session , private madoka::net::SocketEventAdapter {
 public:
  explicit ScissorsTcpSession(Scissors* service,
                              const Service::ChannelPtr& source);
  virtual ~ScissorsTcpSession();

  bool Start() override;
  void Stop() override;

  void OnConnected(madoka::net::AsyncSocket* socket, DWORD error) override;

 private:
  static const size_t kBufferSize = 8192;

  Service::ChannelPtr source_;
  Service::ChannelPtr sink_;
};

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_TCP_SESSION_H_
