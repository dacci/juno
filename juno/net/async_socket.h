// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_ASYNC_SOCKET_H_
#define JUNO_NET_ASYNC_SOCKET_H_

#include <madoka/net/socket.h>

class AsyncSocket : public madoka::net::Socket {
 public:
  AsyncSocket();
  virtual ~AsyncSocket();

  operator HANDLE() const {
    return reinterpret_cast<HANDLE>(descriptor_);
  }

 private:
  friend class AsyncServerSocket;
};

#endif  // JUNO_NET_ASYNC_SOCKET_H_
