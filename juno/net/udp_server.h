// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_UDP_SERVER_H_
#define JUNO_NET_UDP_SERVER_H_

#include <madoka/net/address_info.h>

#include <map>
#include <vector>

#include "net/async_datagram_socket.h"
#include "net/server.h"

class Service;

class UdpServer : public Server, public AsyncDatagramSocket::Listener {
 public:
  UdpServer();
  virtual ~UdpServer();

  bool Setup(const char* address, int port);
  bool Start();
  void Stop();

  void OnReceived(AsyncDatagramSocket* socket, DWORD error, int length);
  void OnReceivedFrom(AsyncDatagramSocket* socket, DWORD error, int length,
                      sockaddr* from, int from_length);
  void OnSent(AsyncDatagramSocket* socket, DWORD error, int length);
  void OnSentTo(AsyncDatagramSocket* socket, DWORD error, int length,
                sockaddr* to, int to_length);

  void SetService(Service* service) {
    service_ = service;
  }

 private:
  static const int kBufferSize = 65536;

#ifdef LEGACY_PLATFORM
  static DWORD CALLBACK Dispatch(void* context);
#else   // LEGACY_PLATFORM
  static void CALLBACK Dispatch(PTP_CALLBACK_INSTANCE instance, void* context);
#endif  // LEGACY_PLATFORM

  madoka::net::AddressInfo resolver_;
  std::vector<AsyncDatagramSocket*> servers_;
  std::map<AsyncDatagramSocket*, char*> buffers_;
  Service* service_;

  LONG count_;
  HANDLE event_;
};

#endif  // JUNO_NET_UDP_SERVER_H_
