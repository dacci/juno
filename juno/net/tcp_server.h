// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_TCP_SERVER_H_
#define JUNO_NET_TCP_SERVER_H_

#include <madoka/net/address_info.h>

#include <vector>

class AsyncServerSocket;
class ServiceProvider;

class TcpServer {
 public:
  TcpServer();
  ~TcpServer();

  bool Setup(const char* address, int port);
  bool Start();
  void Stop();

  void SetService(ServiceProvider* service) {
    service_ = service;
  }

 private:
  madoka::net::AddressInfo resolver_;
  std::vector<AsyncServerSocket*> servers_;
  ServiceProvider* service_;
};

#endif  // JUNO_NET_TCP_SERVER_H_
