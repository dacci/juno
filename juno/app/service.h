// Copyright (c) 2013 dacci.org

#ifndef JUNO_APP_SERVICE_H_
#define JUNO_APP_SERVICE_H_

#include <madoka/net/async_datagram_socket.h>
#include <madoka/net/async_socket.h>

class Service {
 public:
  struct Datagram {
    Service* service;
    madoka::net::AsyncDatagramSocket* socket;
    int data_length;
    void* data;
    int from_length;
    sockaddr* from;
  };

  virtual ~Service() {
  }

  virtual void Stop() = 0;

  virtual bool OnAccepted(madoka::net::AsyncSocket* client) = 0;
  virtual bool OnReceivedFrom(Datagram* datagram) = 0;
  virtual void OnError(DWORD error) = 0;
};

#endif  // JUNO_APP_SERVICE_H_
