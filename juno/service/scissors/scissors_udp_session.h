// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_UDP_SESSION_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_UDP_SESSION_H_

#include <madoka/net/async_datagram_socket.h>
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
  ScissorsUdpSession(Scissors* service,
                     const Scissors::AsyncDatagramSocketPtr& source);
  virtual ~ScissorsUdpSession();

  bool Start() override;
  void Stop() override;

 private:
  static const size_t kBufferSize = 64 * 1024;  // 64 KiB
  static const int kTimeout = 15 * 1000;        // 15 sec

  void OnReceived(const Service::DatagramPtr& datagram) override;

  void OnReceived(madoka::net::AsyncDatagramSocket* socket, DWORD error,
                  void* buffer, int length) override;

  void OnTimeout() override;

  Scissors::AsyncDatagramSocketPtr source_;
  Scissors::AsyncDatagramSocketPtr sink_;
  char buffer_[kBufferSize];
  TimerService::TimerObject timer_;
};

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_UDP_SESSION_H_
