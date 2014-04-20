// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_UDP_SERVER_H_
#define JUNO_NET_UDP_SERVER_H_

#include <madoka/net/resolver.h>
#include <madoka/net/socket_event_listener.h>

#include <map>
#include <vector>

#include "net/server.h"

class Service;

class UdpServer : public Server, public madoka::net::SocketEventAdapter {
 public:
  UdpServer();
  virtual ~UdpServer();

  bool Setup(const char* address, int port) override;
  bool Start() override;
  void Stop() override;

  void OnReceivedFrom(madoka::net::AsyncDatagramSocket* socket, DWORD error,
                      int length, sockaddr* from, int from_length) override;

  void SetService(Service* service) override {
    service_ = service;
  }

 private:
  static const int kBufferSize = 65536;

  static void CALLBACK Dispatch(PTP_CALLBACK_INSTANCE instance, void* context);

  madoka::net::Resolver resolver_;
  std::vector<madoka::net::AsyncDatagramSocket*> servers_;
  std::map<madoka::net::AsyncDatagramSocket*, char*> buffers_;
  Service* service_;

  LONG count_;
  HANDLE event_;
};

#endif  // JUNO_NET_UDP_SERVER_H_
