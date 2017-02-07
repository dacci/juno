// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SERVICE_H_
#define JUNO_SERVICE_SERVICE_H_

#include <winerror.h>

#include <memory>

namespace juno {
namespace io {

class Channel;

namespace net {

struct Datagram;

}  // namespace net
}  // namespace io

namespace service {

struct ServiceConfig;

class __declspec(novtable) Service {
 public:
  virtual ~Service() {}

  virtual bool UpdateConfig(const ServiceConfig* config) = 0;
  virtual void Stop() = 0;

  virtual void OnAccepted(std::unique_ptr<io::Channel>&& client) = 0;
  virtual void OnReceivedFrom(
      std::unique_ptr<io::net::Datagram>&& datagram) = 0;
};

}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SERVICE_H_
