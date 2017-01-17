// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_UDP_SERVER_H_
#define JUNO_SERVICE_UDP_SERVER_H_

#include <base/synchronization/condition_variable.h>
#include <base/synchronization/lock.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "io/net/datagram_channel.h"
#include "io/net/socket_resolver.h"
#include "service/server.h"

namespace juno {
namespace service {

class Service;

class UdpServer : public Server,
                  public io::Channel::Listener,
                  public io::net::DatagramChannel::Listener {
 public:
  UdpServer();
  ~UdpServer();

  bool Setup(const char* address, int port) override;
  bool Start() override;
  void Stop() override;

  void OnRead(io::Channel* channel, HRESULT result, void* buffer,
              int length) override;
  void OnRead(io::net::DatagramChannel* channel, HRESULT result, void* buffer,
              int length, const void* from, int from_length) override;
  void OnWritten(io::Channel* channel, HRESULT result, void* buffer,
                 int length) override;

  void SetService(Service* service) override {
    service_ = service;
  }

 private:
  typedef std::pair<UdpServer*, io::net::DatagramChannel*> ServerSocketPair;

  static const int kBufferSize = 65536;

  void DeleteServer(io::net::DatagramChannel* server);
  static void CALLBACK DeleteServerImpl(PTP_CALLBACK_INSTANCE instance,
                                        void* context);
  void DeleteServerImpl(io::net::DatagramChannel* server);

  io::net::SocketResolver resolver_;
  std::vector<std::shared_ptr<io::net::DatagramChannel>> servers_;
  std::map<io::net::DatagramChannel*, std::unique_ptr<char[]>> buffers_;
  Service* service_;

  base::Lock lock_;
  base::ConditionVariable empty_;

  UdpServer(const UdpServer&) = delete;
  UdpServer& operator=(const UdpServer&) = delete;
};

}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_UDP_SERVER_H_
