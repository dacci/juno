// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_SERVICE_PROVIDER_H_
#define JUNO_NET_SERVICE_PROVIDER_H_

#include "net/async_server_socket.h"

class ServiceProvider : public AsyncServerSocket::Listener {
 public:
  virtual ~ServiceProvider() {
  }

  virtual bool Setup(HKEY key) = 0;
  virtual void Stop() = 0;
};

#endif  // JUNO_NET_SERVICE_PROVIDER_H_
