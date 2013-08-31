// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_ASYNC_SERVER_SOCKET_H_
#define JUNO_NET_ASYNC_SERVER_SOCKET_H_

#include <madoka/net/server_socket.h>

class AsyncSocket;

class AsyncServerSocket : public madoka::net::ServerSocket {
 public:
  class Listener {
   public:
    virtual ~Listener() {}

    virtual void OnAccepted(AsyncServerSocket* server, AsyncSocket* client,
                            DWORD error) = 0;
  };

  AsyncServerSocket();
  virtual ~AsyncServerSocket();

  virtual void Close();

  bool AcceptAsync(Listener* listener);
  OVERLAPPED* BeginAccept(HANDLE event);
  AsyncSocket* EndAccept(OVERLAPPED* overlapped);

  operator HANDLE() const {
    return reinterpret_cast<HANDLE>(descriptor_);
  }

 private:
  struct AcceptContext : OVERLAPPED {
    AsyncServerSocket* server;
    AsyncSocket* client;
    char* buffer;
    Listener* listener;
    HANDLE event;
  };

  AcceptContext* CreateAcceptContext();
  void DestroyAcceptContext(AcceptContext* context);

#ifdef LEGACY_PLATFORM
  static DWORD CALLBACK AsyncWork(void* param);
  static void CALLBACK OnAccepted(DWORD error, DWORD bytes,
                                  OVERLAPPED* overlapped);
#else   // LEGACY_PLATFORM
  static void CALLBACK AsyncWork(PTP_CALLBACK_INSTANCE instance,
                                  void* param);
  static void CALLBACK OnAccepted(PTP_CALLBACK_INSTANCE instance,
                                  void* self,
                                  void* overlapped,
                                  ULONG error,
                                  ULONG_PTR bytes,
                                  PTP_IO io);
#endif  // LEGACY_PLATFORM

  bool initialized_;
  PTP_IO io_;
  int family_;
  int protocol_;
};

#endif  // JUNO_NET_ASYNC_SERVER_SOCKET_H_
