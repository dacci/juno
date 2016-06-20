// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_UDP_SERVER_H_
#define JUNO_NET_UDP_SERVER_H_

#include <madoka/net/async_socket.h>

#include <base/synchronization/condition_variable.h>
#include <base/synchronization/lock.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "net/server.h"
#include "net/socket_resolver.h"

struct Datagram;
class Service;

class UdpServer : public Server, public madoka::net::AsyncSocket::Listener {
 public:
  UdpServer();
  virtual ~UdpServer();

  bool Setup(const char* address, int port) override;
  bool Start() override;
  void Stop() override;

  void OnReceived(madoka::net::AsyncSocket* socket, HRESULT result,
                  void* buffer, int length, int flags) override;
  void OnReceivedFrom(madoka::net::AsyncSocket* socket, HRESULT result,
                      void* buffer, int length, int flags,
                      const sockaddr* address, int address_length) override;

  void OnConnected(madoka::net::AsyncSocket* socket, HRESULT result,
                   const addrinfo* end_point) override {}
  void OnSent(madoka::net::AsyncSocket* socket, HRESULT result, void* buffer,
              int length) override {}
  void OnSentTo(madoka::net::AsyncSocket* socket, HRESULT result, void* buffer,
                int length, const sockaddr* address,
                int address_length) override {}

  void SetService(Service* service) override {
    service_ = service;
  }

 private:
  typedef std::pair<UdpServer*, madoka::net::AsyncSocket*>
      ServerSocketPair;

  static const int kBufferSize = 65536;

  void DeleteServer(madoka::net::AsyncSocket* server);
  static void CALLBACK DeleteServerImpl(PTP_CALLBACK_INSTANCE instance,
                                        void* context);
  void DeleteServerImpl(madoka::net::AsyncSocket* server);

  SocketResolver resolver_;
  std::vector<std::shared_ptr<madoka::net::AsyncSocket>> servers_;
  std::map<madoka::net::AsyncSocket*, std::unique_ptr<char[]>> buffers_;
  Service* service_;

  base::Lock lock_;
  base::ConditionVariable empty_;
};

#endif  // JUNO_NET_UDP_SERVER_H_
