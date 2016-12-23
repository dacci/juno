// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_TCP_SERVER_H_
#define JUNO_NET_TCP_SERVER_H_

#include <base/synchronization/condition_variable.h>
#include <base/synchronization/lock.h>

#include <memory>
#include <utility>
#include <vector>

#include "net/async_server_socket.h"
#include "net/server.h"
#include "net/socket_resolver.h"
#include "service/service.h"

class TcpServer : public Server, private AsyncServerSocket::Listener {
 public:
  class __declspec(novtable) ChannelCustomizer {
   public:
    virtual ~ChannelCustomizer() {}

    virtual Service::ChannelPtr Customize(
        const Service::ChannelPtr& channel) = 0;
  };

  TcpServer();
  virtual ~TcpServer();

  bool Setup(const char* address, int port) override;
  bool Start() override;
  void Stop() override;

  void SetChannelCustomizer(ChannelCustomizer* customizer) {
    base::AutoLock guard(lock_);
    channel_customizer_ = customizer;
  }

  void SetService(Service* service) override {
    service_ = service;
  }

 private:
  typedef std::pair<TcpServer*, AsyncServerSocket*> ServerSocketPair;

  void DeleteServer(AsyncServerSocket* server);
  static void CALLBACK DeleteServerImpl(PTP_CALLBACK_INSTANCE instance,
                                        void* param);
  void DeleteServerImpl(AsyncServerSocket* server);

  void OnAccepted(AsyncServerSocket* server, HRESULT result,
                  AsyncServerSocket::Context* context) override;

  ChannelCustomizer* channel_customizer_;
  SocketResolver resolver_;
  std::vector<std::unique_ptr<AsyncServerSocket>> servers_;
  Service* service_;

  base::Lock lock_;
  base::ConditionVariable empty_;

  TcpServer(const TcpServer&) = delete;
  TcpServer& operator=(const TcpServer&) = delete;
};

#endif  // JUNO_NET_TCP_SERVER_H_
