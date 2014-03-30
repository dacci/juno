// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_TCP_SERVER_H_
#define JUNO_NET_TCP_SERVER_H_

#include <madoka/net/resolver.h>

#include <vector>

#include "net/async_server_socket.h"
#include "net/server.h"

class Service;

class TcpServer : public Server, public AsyncServerSocket::Listener {
 public:
  TcpServer();
  virtual ~TcpServer();

  bool Setup(const char* address, int port);
  bool Start();
  void Stop();

  void OnAccepted(AsyncServerSocket* server, AsyncSocket* client, DWORD error);

  void SetService(Service* service) {
    service_ = service;
  }

 private:
  madoka::net::Resolver resolver_;
  std::vector<AsyncServerSocket*> servers_;
  Service* service_;

  LONG count_;
  HANDLE event_;
};

#endif  // JUNO_NET_TCP_SERVER_H_
