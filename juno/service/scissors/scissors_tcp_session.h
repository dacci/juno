// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_TCP_SESSION_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_TCP_SESSION_H_

#include "service/scissors/scissors.h"

class ScissorsTcpSession
    : public Scissors::Session , private madoka::net::AsyncSocket::Listener {
 public:
  explicit ScissorsTcpSession(Scissors* service,
                              const Service::ChannelPtr& source);
  virtual ~ScissorsTcpSession();

  bool Start() override;
  void Stop() override;

  void OnConnected(madoka::net::AsyncSocket* socket, HRESULT result,
                   const addrinfo* end_point) override;

  void OnReceived(madoka::net::AsyncSocket* socket, HRESULT result,
                  void* buffer, int length, int flags) override {}
  void OnReceivedFrom(madoka::net::AsyncSocket* socket, HRESULT result,
                      void* buffer, int length, int flags,
                      const sockaddr* address, int address_length) override {}
  void OnSent(madoka::net::AsyncSocket* socket, HRESULT result, void* buffer,
              int length) override {}
  void OnSentTo(madoka::net::AsyncSocket* socket, HRESULT result, void* buffer,
                int length, const sockaddr* address,
                int address_length) override {}

 private:
  static const size_t kBufferSize = 8192;

  Service::ChannelPtr source_;
  Service::ChannelPtr sink_;
};

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_TCP_SESSION_H_
