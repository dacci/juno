// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_TCP_SERVER_H_
#define JUNO_NET_TCP_SERVER_H_

#include <madoka/concurrent/critical_section.h>
#include <madoka/net/address_info.h>

#include <vector>

#include "net/async_server_socket.h"

class ServiceProvider;

class TcpServer : public AsyncServerSocket::Listener {
 public:
  TcpServer();
  virtual ~TcpServer();

  bool Setup(const char* address, int port);
  bool Start();
  void Stop();

  void OnAccepted(AsyncServerSocket* server, AsyncSocket* client, DWORD error);

  void SetService(ServiceProvider* service) {
    service_ = service;
  }

 private:
  madoka::net::AddressInfo resolver_;
  std::vector<AsyncServerSocket*> servers_;
  ServiceProvider* service_;

  int count_;
  HANDLE event_;
  madoka::concurrent::CriticalSection critical_section_;
};

#endif  // JUNO_NET_TCP_SERVER_H_
