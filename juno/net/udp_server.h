// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_UDP_SERVER_H_
#define JUNO_NET_UDP_SERVER_H_

#pragma warning(push, 3)
#pragma warning(disable : 4244)
#include <base/synchronization/condition_variable.h>
#include <base/synchronization/lock.h>
#pragma warning(pop)

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "net/datagram_channel.h"
#include "net/server.h"
#include "net/socket_resolver.h"

struct Datagram;
class Service;

class UdpServer : public Server,
                  public Channel::Listener,
                  public DatagramChannel::Listener {
 public:
  UdpServer();
  virtual ~UdpServer();

  bool Setup(const char* address, int port) override;
  bool Start() override;
  void Stop() override;

  void OnRead(Channel* channel, HRESULT result, void* buffer,
              int length) override;
  void OnRead(DatagramChannel* channel, HRESULT result, void* buffer,
              int length, const void* from, int from_length) override;
  void OnWritten(Channel* channel, HRESULT result, void* buffer,
                 int length) override;

  void SetService(Service* service) override {
    service_ = service;
  }

 private:
  typedef std::pair<UdpServer*, DatagramChannel*> ServerSocketPair;

  static const int kBufferSize = 65536;

  void DeleteServer(DatagramChannel* server);
  static void CALLBACK DeleteServerImpl(PTP_CALLBACK_INSTANCE instance,
                                        void* context);
  void DeleteServerImpl(DatagramChannel* server);

  SocketResolver resolver_;
  std::vector<std::shared_ptr<DatagramChannel>> servers_;
  std::map<DatagramChannel*, std::unique_ptr<char[]>> buffers_;
  Service* service_;

  base::Lock lock_;
  base::ConditionVariable empty_;

  UdpServer(const UdpServer&) = delete;
  UdpServer& operator=(const UdpServer&) = delete;
};

#endif  // JUNO_NET_UDP_SERVER_H_
