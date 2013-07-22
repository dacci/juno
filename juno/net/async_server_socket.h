// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_ASYNC_SERVER_SOCKET_H_
#define JUNO_NET_ASYNC_SERVER_SOCKET_H_

#include <madoka/net/server_socket.h>

class AsyncSocket;

class AsyncServerSocket : public madoka::net::ServerSocket {
 public:
  struct AcceptContext : OVERLAPPED {
    AcceptContext() : OVERLAPPED(), peer(), buffer(), bytes() {
    }

    AsyncSocket* peer;
    char* buffer;
    DWORD bytes;
  };

  AsyncServerSocket();
  virtual ~AsyncServerSocket();

  bool Bind(const addrinfo* end_point);

  AcceptContext* BeginAsyncAccept(HANDLE event);
  AsyncSocket* EndAsyncAccept(AcceptContext* context);

  operator HANDLE() const {
    return reinterpret_cast<HANDLE>(descriptor_);
  }

 private:
  int family_;
  int protocol_;
};

#endif  // JUNO_NET_ASYNC_SERVER_SOCKET_H_
