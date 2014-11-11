// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_TCP_SERVER_H_
#define JUNO_NET_TCP_SERVER_H_

#include <madoka/concurrent/condition_variable.h>
#include <madoka/concurrent/critical_section.h>
#include <madoka/net/resolver.h>
#include <madoka/net/async_server_socket.h>
#include <madoka/net/socket_event_listener.h>

#include <memory>
#include <utility>
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
  typedef std::pair<TcpServer*, madoka::net::AsyncServerSocket*>
      ServerSocketPair;

  void DeleteServer(madoka::net::AsyncServerSocket* server);
  static void CALLBACK DeleteServerImpl(PTP_CALLBACK_INSTANCE instance,
                                        void* param);
  void DeleteServerImpl(madoka::net::AsyncServerSocket* server);

  madoka::net::Resolver resolver_;
  std::vector<std::unique_ptr<madoka::net::AsyncServerSocket>> servers_;
  Service* service_;

  madoka::concurrent::CriticalSection lock_;
  madoka::concurrent::ConditionVariable empty_;
};

#endif  // JUNO_NET_TCP_SERVER_H_
