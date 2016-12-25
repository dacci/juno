// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SERVICE_H_
#define JUNO_SERVICE_SERVICE_H_

#include <winerror.h>

#include "io/channel.h"
#include "io/net/datagram.h"
#include "service/service_config.h"

class __declspec(novtable) Service {
 public:
  virtual ~Service() {}

  virtual bool UpdateConfig(const ServiceConfigPtr& config) = 0;
  virtual void Stop() = 0;

  virtual void OnAccepted(const ChannelPtr& client) = 0;
  virtual void OnReceivedFrom(const DatagramPtr& datagram) = 0;
  virtual void OnError(HRESULT result) = 0;
};

#ifndef JUNO_NO_SERVICE_PTR
#include <memory>
typedef std::unique_ptr<Service> ServicePtr;
#endif  // JUNO_NO_SERVICE_PTR

#endif  // JUNO_SERVICE_SERVICE_H_
