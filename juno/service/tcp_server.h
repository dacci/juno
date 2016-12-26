// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_TCP_SERVER_H_
#define JUNO_SERVICE_TCP_SERVER_H_

#include <base/synchronization/condition_variable.h>
#include <base/synchronization/lock.h>

#include <memory>
#include <utility>
#include <vector>

#include "io/net/async_server_socket.h"
#include "io/net/socket_resolver.h"
#include "service/server.h"
#include "service/service.h"

namespace juno {
namespace service {

using ::juno::io::net::AsyncServerSocket;

class TcpServer : public Server, private AsyncServerSocket::Listener {
 public:
  class __declspec(novtable) ChannelCustomizer {
   public:
    virtual ~ChannelCustomizer() {}

    virtual io::ChannelPtr Customize(const io::ChannelPtr& channel) = 0;
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
  io::net::SocketResolver resolver_;
  std::vector<std::unique_ptr<AsyncServerSocket>> servers_;
  Service* service_;

  base::Lock lock_;
  base::ConditionVariable empty_;

  TcpServer(const TcpServer&) = delete;
  TcpServer& operator=(const TcpServer&) = delete;
};

}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_TCP_SERVER_H_
