// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SERVICE_H_
#define JUNO_SERVICE_SERVICE_H_

#include <madoka/net/async_datagram_socket.h>
#include <madoka/net/async_socket.h>

#include <memory>

#include "net/channel.h"
#include "service/service_config.h"

class Service {
 public:
  typedef std::shared_ptr<Channel> ChannelPtr;

  struct Datagram {
    Service* service;
    madoka::net::AsyncDatagramSocket* socket;
    int data_length;
    void* data;
    int from_length;
    sockaddr* from;
  };

  virtual ~Service() {}

  virtual bool UpdateConfig(const ServiceConfigPtr& config) = 0;
  virtual void Stop() = 0;

  virtual void OnAccepted(const ChannelPtr& client) = 0;
  virtual bool OnReceivedFrom(Datagram* datagram) = 0;
  virtual void OnError(DWORD error) = 0;
};

typedef std::unique_ptr<Service> ServicePtr;

#endif  // JUNO_SERVICE_SERVICE_H_
