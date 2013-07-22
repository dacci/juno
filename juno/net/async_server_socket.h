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

  virtual void Close();

  bool SetThreadpool(PTP_CALLBACK_ENVIRON environment);

  bool Bind(const addrinfo* end_point);

  AcceptContext* BeginAccept(HANDLE event);
  AcceptContext* BeginAccept();
  AsyncSocket* EndAccept(AcceptContext* context);

  operator HANDLE() const {
    return reinterpret_cast<HANDLE>(descriptor_);
  }

 private:
  static void CALLBACK OnAccepted(PTP_CALLBACK_INSTANCE instance,
                                  PVOID context, PVOID overlapped,
                                  ULONG io_result, ULONG_PTR bytes, PTP_IO io);
  void OnAccepted(AsyncSocket* peer, ULONG error);

  PTP_IO io_;
  int family_;
  int protocol_;
};

#endif  // JUNO_NET_ASYNC_SERVER_SOCKET_H_
