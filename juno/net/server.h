// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_SERVER_H_
#define JUNO_NET_SERVER_H_

class Service;

class Server {
 public:
  virtual ~Server() {}

  virtual bool Setup(const char* address, int port) = 0;
  virtual bool Start() = 0;
  virtual void Stop() = 0;

  virtual void SetService(Service* service) = 0;
};

#endif  // JUNO_NET_SERVER_H_
