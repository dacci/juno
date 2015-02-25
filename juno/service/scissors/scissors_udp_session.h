// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_UDP_SESSION_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_UDP_SESSION_H_

#include <madoka/net/socket_event_listener.h>

#include <memory>

#include "misc/timer_service.h"
#include "service/service.h"
#include "service/scissors/scissors.h"

class ScissorsUdpSession
    : public Scissors::UdpSession,
      private madoka::net::SocketEventAdapter,
      private TimerService::Callback {
 public:
  ScissorsUdpSession(Scissors* service, const Scissors::AsyncSocketPtr& source);
  virtual ~ScissorsUdpSession();

  bool Start() override;
  void Stop() override;

 private:
  static const size_t kBufferSize = 64 * 1024;  // 64 KiB
  static const int kTimeout = 15 * 1000;        // 15 sec

  void OnReceived(const Service::DatagramPtr& datagram) override;

  void OnReceived(madoka::net::AsyncSocket* socket, DWORD error, void* buffer,
                  int length) override;

  void OnTimeout() override;

  Scissors::AsyncSocketPtr source_;
  Scissors::AsyncSocketPtr sink_;
  sockaddr_storage address_;
  int address_length_;
  char buffer_[kBufferSize];
  TimerService::TimerObject timer_;
};

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_UDP_SESSION_H_
