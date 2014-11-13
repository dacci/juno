// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_TCP_SESSION_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_TCP_SESSION_H_

#include <madoka/net/resolver.h>
#include <madoka/net/socket_event_listener.h>

#include <memory>
#include <vector>

#include "misc/security/security_buffer.h"
#include "service/scissors/scissors.h"

class ScissorsTcpSession
    : public Scissors::Session, public madoka::net::SocketEventAdapter {
 public:
  explicit ScissorsTcpSession(Scissors* service,
                              const Service::ChannelPtr& client);
  virtual ~ScissorsTcpSession();

  bool Start();
  void Stop() override;

  void OnConnected(madoka::net::AsyncSocket* socket, DWORD error) override;

 private:
  static const size_t kBufferSize = 8192;

  madoka::net::Resolver resolver_;
  Service::ChannelPtr client_;
  std::shared_ptr<madoka::net::AsyncSocket> remote_;
};

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_TCP_SESSION_H_
