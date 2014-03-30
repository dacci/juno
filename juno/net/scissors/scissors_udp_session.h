// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_SCISSORS_SCISSORS_UDP_SESSION_H_
#define JUNO_NET_SCISSORS_SCISSORS_UDP_SESSION_H_

#include <madoka/net/resolver.h>

#include <memory>

#include "app/service.h"
#include "net/async_datagram_socket.h"
#include "net/scissors/scissors.h"

class ScissorsUdpSession
    : public Scissors::Session, public AsyncDatagramSocket::Listener {
 public:
  ScissorsUdpSession(Scissors* service, Service::Datagram* datagram);
  virtual ~ScissorsUdpSession();

  bool Start();
  void Stop();

  void OnReceived(AsyncDatagramSocket* socket, DWORD error, int length);
  void OnReceivedFrom(AsyncDatagramSocket* socket, DWORD error, int length,
                      sockaddr* from, int from_length);
  void OnSent(AsyncDatagramSocket* socket, DWORD error, int length);
  void OnSentTo(AsyncDatagramSocket* socket, DWORD error, int length,
                sockaddr* to, int to_length);

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
  std::unique_ptr<AsyncDatagramSocket> remote_;
  std::unique_ptr<char[]> buffer_;
  PTP_TIMER timer_;
};

#endif  // JUNO_NET_SCISSORS_SCISSORS_UDP_SESSION_H_
