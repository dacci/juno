// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_TCP_SERVER_H_
#define JUNO_NET_TCP_SERVER_H_

#include <madoka/net/resolver.h>
#include <madoka/net/async_server_socket.h>
#include <madoka/net/socket_event_listener.h>

#include <vector>

#include "net/server.h"

class Service;

class TcpServer : public Server, public madoka::net::SocketEventAdapter {
 public:
  TcpServer();
  virtual ~TcpServer();

  bool Setup(const char* address, int port) override;
  bool Start() override;
  void Stop() override;

  void OnAccepted(madoka::net::AsyncServerSocket* server,
                  madoka::net::AsyncSocket* client, DWORD error) override;

  void SetService(Service* service) override {
    service_ = service;
  }

 private:
  madoka::net::Resolver resolver_;
  std::vector<madoka::net::AsyncServerSocket*> servers_;
  Service* service_;

  LONG count_;
  HANDLE event_;
};

#endif  // JUNO_NET_TCP_SERVER_H_
