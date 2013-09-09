// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_SERVICE_PROVIDER_H_
#define JUNO_NET_SERVICE_PROVIDER_H_

#include <windows.h>

struct sockaddr;

class AsyncSocket;
class AsyncDatagramSocket;

class ServiceProvider {
 public:
  virtual ~ServiceProvider() {
  }

  virtual bool Setup(HKEY key) = 0;
  virtual void Stop() = 0;

  virtual bool OnAccepted(AsyncSocket* client) = 0;
  virtual void OnReceivedFrom(AsyncDatagramSocket* socket, void* data,
                              int length, sockaddr* from, int from_length) = 0;
  virtual void OnError(DWORD error) = 0;
};

#endif  // JUNO_NET_SERVICE_PROVIDER_H_
