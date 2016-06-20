// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_TCP_SERVER_H_
#define JUNO_NET_TCP_SERVER_H_

#include <madoka/net/async_server_socket.h>

#include <base/synchronization/condition_variable.h>
#include <base/synchronization/lock.h>

#include <memory>
#include <utility>
#include <vector>

#include "net/channel.h"
#include "net/server.h"
#include "net/socket_resolver.h"
#include "service/service.h"

class TcpServer
    : public Server, private madoka::net::AsyncServerSocket::Listener {
 public:
  class ChannelFactory {
   public:
    virtual Service::ChannelPtr CreateChannel(
        const std::shared_ptr<madoka::net::AsyncSocket>& socket) = 0;
  };

  TcpServer();
  virtual ~TcpServer();

  bool Setup(const char* address, int port) override;
  bool Start() override;
  void Stop() override;

  void SetChannelFactory(ChannelFactory* channel_factory);

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

  void OnAccepted(
    madoka::net::AsyncServerSocket* server, HRESULT result,
    madoka::net::AsyncServerSocket::Context* context) override;

  ChannelFactory* channel_factory_;
  SocketResolver resolver_;
  std::vector<std::unique_ptr<madoka::net::AsyncServerSocket>> servers_;
  Service* service_;

  base::Lock lock_;
  base::ConditionVariable empty_;
};

#endif  // JUNO_NET_TCP_SERVER_H_
