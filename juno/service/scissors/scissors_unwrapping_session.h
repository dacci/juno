// Copyright (c) 2015 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_UNWRAPPING_SESSION_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_UNWRAPPING_SESSION_H_

#include <stdint.h>

#include <madoka/concurrent/critical_section.h>
#include <madoka/net/socket_event_listener.h>

#include <string>

#include "net/channel.h"
#include "service/scissors/scissors.h"

class ScissorsUnwrappingSession
    : public Scissors::Session,
      private Channel::Listener,
      private madoka::net::SocketEventAdapter {
 public:
  typedef std::shared_ptr<madoka::net::AsyncSocket> AsyncSocketPtr;

  explicit ScissorsUnwrappingSession(Scissors* service);
  virtual ~ScissorsUnwrappingSession();

  bool Start() override;
  void Stop() override;

  void SetSource(const Service::ChannelPtr& source) {
    source_ = source;
  }

  void SetSink(const AsyncSocketPtr& sink) {
    sink_ = sink;
  }

  void SetSinkAddress(const sockaddr_storage& address, int length) {
    memmove(&sink_address_, &address, length);
    sink_address_length_ = length;
  }

 private:
  static const int kBufferSize = 65535;

  void ProcessBuffer();

  void OnRead(Channel* channel, DWORD error, void* buffer,
              int length) override;
  void OnWritten(Channel* channel, DWORD error, void* buffer,
                 int length) override;
  void OnSent(madoka::net::AsyncSocket* socket, DWORD error, void* buffer,
              int length) override;
  void OnSentTo(madoka::net::AsyncSocket* socket, DWORD error, void* buffer,
                int length, sockaddr* to, int to_length) override;

  Service::ChannelPtr source_;
  AsyncSocketPtr sink_;
  sockaddr_storage sink_address_;
  int sink_address_length_;
  char buffer_[2 + kBufferSize];  // header + payload
  int packet_length_;
  std::string received_;
};

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_UNWRAPPING_SESSION_H_
