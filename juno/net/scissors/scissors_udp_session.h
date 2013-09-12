// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_SCISSORS_SCISSORS_UDP_SESSION_H_
#define JUNO_NET_SCISSORS_SCISSORS_UDP_SESSION_H_

#include "net/async_datagram_socket.h"
#include "net/service_provider.h"
#include "net/scissors/scissors.h"

class ScissorsUdpSession
    : public Scissors::Session, public AsyncDatagramSocket::Listener {
 public:
  ScissorsUdpSession(Scissors* service, ServiceProvider::Datagram* datagram);
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
  static const size_t kBufferSize = 8192;

  static void CALLBACK DeleteThis(PTP_CALLBACK_INSTANCE instance, void* param);

  Scissors* const service_;
  ServiceProvider::Datagram* datagram_;
  AsyncDatagramSocket* remote_;
  char* buffer_;
};

#endif  // JUNO_NET_SCISSORS_SCISSORS_UDP_SESSION_H_
