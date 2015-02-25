// Copyright (c) 2015 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_WRAPPING_SESSION_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_WRAPPING_SESSION_H_

#include <madoka/net/socket_event_listener.h>

#include <memory>

#include "misc/timer_service.h"
#include "net/channel.h"
#include "service/scissors/scissors.h"

class ScissorsUnwrappingSession;

class ScissorsWrappingSession
    : public Scissors::UdpSession,
      private Channel::Listener,
      private madoka::net::SocketEventAdapter,
      private TimerService::Callback {
 public:
  typedef std::shared_ptr<madoka::net::AsyncSocket> AsyncSocketPtr;

  explicit ScissorsWrappingSession(Scissors* service);
  virtual ~ScissorsWrappingSession();

  bool Start() override;
  void Stop() override;

  void SetSource(const AsyncSocketPtr& source) {
    source_ = source;
  }

  void SetSink(const Service::ChannelPtr& sink) {
    sink_ = sink;
  }

  void SetSourceAddress(const sockaddr_storage& address, int length) {
    memmove(&source_address_, &address, length);
    source_address_length_ = length;
  }

 private:
  static const int kLengthOffset = 2;
  static const int kDatagramSize = 65535;
  static const int kTimeout = 60 * 1000;  // 60 sec.

  void OnReceived(const Service::DatagramPtr& datagram) override;

  void OnConnected(madoka::net::AsyncSocket* socket, DWORD error) override;
  void OnReceived(madoka::net::AsyncSocket* socket, DWORD error, void* buffer,
                  int length) override;

  void OnRead(Channel* channel, DWORD error, void* buffer, int length) override;
  void OnWritten(Channel* channel, DWORD error, void* buffer,
                 int length) override;

  void OnTimeout() override;

  TimerService::TimerObject timer_;
  AsyncSocketPtr source_;
  Service::ChannelPtr sink_;
  sockaddr_storage source_address_;
  int source_address_length_;
  char buffer_[kLengthOffset + kDatagramSize];
};

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_WRAPPING_SESSION_H_
