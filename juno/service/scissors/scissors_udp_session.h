// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_UDP_SESSION_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_UDP_SESSION_H_

#include <memory>

#include "misc/timer_service.h"
#include "service/service.h"
#include "service/scissors/scissors.h"

class ScissorsUdpSession
    : public Scissors::UdpSession,
      private madoka::net::AsyncSocket::Listener,
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

  void OnReceived(madoka::net::AsyncSocket* socket, HRESULT result,
                  void* buffer, int length, int flags) override;

  void OnConnected(madoka::net::AsyncSocket* socket, HRESULT result,
                   const addrinfo* end_point) override {}
  void OnReceivedFrom(madoka::net::AsyncSocket* socket, HRESULT result,
                      void* buffer, int length, int flags,
                      const sockaddr* address, int address_length) override {}
  void OnSent(madoka::net::AsyncSocket* socket, HRESULT result, void* buffer,
              int length) override {}
  void OnSentTo(madoka::net::AsyncSocket* socket, HRESULT result, void* buffer,
                int length, const sockaddr* address,
                int address_length) override {}

  void OnTimeout() override;

  Scissors::AsyncSocketPtr source_;
  Scissors::AsyncSocketPtr sink_;
  sockaddr_storage address_;
  int address_length_;
  char buffer_[kBufferSize];
  TimerService::TimerObject timer_;
};

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_UDP_SESSION_H_
