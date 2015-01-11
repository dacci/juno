// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SERVICE_H_
#define JUNO_SERVICE_SERVICE_H_

#include <madoka/net/async_socket.h>

#include <memory>

#include "service/service_config.h"

class Channel;
struct Datagram;

class Service {
 public:
  typedef std::shared_ptr<Channel> ChannelPtr;
  typedef std::shared_ptr<Datagram> DatagramPtr;

  virtual ~Service() {}

  virtual bool UpdateConfig(const ServiceConfigPtr& config) = 0;
  virtual void Stop() = 0;

  virtual void OnAccepted(const ChannelPtr& client) = 0;
  virtual void OnReceivedFrom(const DatagramPtr& datagram) = 0;
  virtual void OnError(DWORD error) = 0;
};

typedef std::unique_ptr<Service> ServicePtr;

#endif  // JUNO_SERVICE_SERVICE_H_
