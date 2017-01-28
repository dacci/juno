// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SERVICE_H_
#define JUNO_SERVICE_SERVICE_H_

#include <winerror.h>

#include <memory>

#include "io/channel.h"
#include "io/net/datagram.h"
#include "service/service_config.h"

namespace juno {
namespace service {

class __declspec(novtable) Service {
 public:
  virtual ~Service() {}

  virtual bool UpdateConfig(const ServiceConfig* config) = 0;
  virtual void Stop() = 0;

  virtual void OnAccepted(const io::ChannelPtr& client) = 0;
  virtual void OnReceivedFrom(
      std::unique_ptr<io::net::Datagram>&& datagram) = 0;
};

}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SERVICE_H_
