// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_TCP_SESSION_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_TCP_SESSION_H_

#include "service/scissors/scissors.h"

class ScissorsTcpSession : public Scissors::Session,
                           private SocketChannel::Listener {
 public:
  ScissorsTcpSession(Scissors* service, const Service::ChannelPtr& source);
  ~ScissorsTcpSession();

  bool Start() override;
  void Stop() override;

  void OnConnected(SocketChannel* channel, HRESULT result) override;
  void OnClosed(SocketChannel* /*channel*/, HRESULT /*result*/) override {}

 private:
  static const size_t kBufferSize = 8192;

  Service::ChannelPtr source_;
  Service::ChannelPtr sink_;

  ScissorsTcpSession(const ScissorsTcpSession&) = delete;
  ScissorsTcpSession& operator=(const ScissorsTcpSession&) = delete;
};

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_TCP_SESSION_H_
