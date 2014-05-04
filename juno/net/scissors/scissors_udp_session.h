// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_SCISSORS_SCISSORS_UDP_SESSION_H_
#define JUNO_NET_SCISSORS_SCISSORS_UDP_SESSION_H_

#include <madoka/net/resolver.h>
#include <madoka/net/socket_event_listener.h>

#include <memory>

#include "app/service.h"
#include "net/scissors/scissors.h"

class ScissorsUdpSession
    : public Scissors::Session, public madoka::net::SocketEventAdapter {
 public:
  ScissorsUdpSession(Scissors* service, Service::Datagram* datagram);
  virtual ~ScissorsUdpSession();

  bool Start();
  void Stop() override;

  void OnReceived(madoka::net::AsyncDatagramSocket* socket, DWORD error,
                  void* buffer, int length) override;
  void OnSent(madoka::net::AsyncDatagramSocket* socket, DWORD error,
              void* buffer, int length) override;
  void OnSentTo(madoka::net::AsyncDatagramSocket* socket, DWORD error,
                void* buffer, int length, sockaddr* to, int to_length) override;

 private:
  static const size_t kBufferSize = 64 * 1024;  // 64 KiB
  static const int kTimeout = 15 * 1000;        // 15 sec

  static void CALLBACK OnTimeout(PTP_CALLBACK_INSTANCE instance, PVOID param,
                                 PTP_TIMER timer);
  static void CALLBACK DeleteThis(PTP_CALLBACK_INSTANCE instance, void* param);

  static FILETIME kTimerDueTime;

  Scissors* const service_;
  madoka::net::Resolver resolver_;
  Service::Datagram* datagram_;
  std::unique_ptr<madoka::net::AsyncDatagramSocket> remote_;
  std::unique_ptr<char[]> buffer_;
  PTP_TIMER timer_;
};

#endif  // JUNO_NET_SCISSORS_SCISSORS_UDP_SESSION_H_
