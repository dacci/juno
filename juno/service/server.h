// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SERVER_H_
#define JUNO_SERVICE_SERVER_H_

namespace juno {
namespace service {

class Service;

class __declspec(novtable) Server {
 public:
  virtual ~Server() {}

  virtual bool Setup(const char* address, int port) = 0;
  virtual bool Start() = 0;
  virtual void Stop() = 0;

  virtual void SetService(Service* service) = 0;
};

}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SERVER_H_
