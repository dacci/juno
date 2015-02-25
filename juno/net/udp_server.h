// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_UDP_SERVER_H_
#define JUNO_NET_UDP_SERVER_H_

#include <madoka/concurrent/condition_variable.h>
#include <madoka/concurrent/critical_section.h>
#include <madoka/net/resolver.h>
#include <madoka/net/socket_event_listener.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "net/server.h"

struct Datagram;
class Service;

class UdpServer : public Server, public madoka::net::SocketEventAdapter {
 public:
  UdpServer();
  virtual ~UdpServer();

  bool Setup(const char* address, int port) override;
  bool Start() override;
  void Stop() override;

  void OnReceived(madoka::net::AsyncSocket* socket, DWORD error, void* buffer,
                  int length) override;
  void OnReceivedFrom(madoka::net::AsyncSocket* socket, DWORD error,
                      void* buffer, int length, sockaddr* from,
                      int from_length) override;

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

  madoka::net::Resolver resolver_;
  std::vector<std::shared_ptr<madoka::net::AsyncSocket>> servers_;
  std::map<madoka::net::AsyncSocket*, std::unique_ptr<char[]>> buffers_;
  Service* service_;

  madoka::concurrent::CriticalSection lock_;
  madoka::concurrent::ConditionVariable empty_;
};

#endif  // JUNO_NET_UDP_SERVER_H_
