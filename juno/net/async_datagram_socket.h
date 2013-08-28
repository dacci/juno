// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_ASYNC_DATAGRAM_SOCKET_H_
#define JUNO_NET_ASYNC_DATAGRAM_SOCKET_H_

#include <madoka/net/datagram_socket.h>

class AsyncDatagramSocket : public madoka::net::DatagramSocket {
 public:
  class Listener {
   public:
    virtual ~Listener() {}

    virtual void OnReceived(AsyncDatagramSocket* socket, DWORD error,
                            int length) = 0;
    virtual void OnReceivedFrom(AsyncDatagramSocket* socket, DWORD error,
                                int length, sockaddr* from,
                                int from_length) = 0;
    virtual void OnSent(AsyncDatagramSocket* socket, DWORD error,
                        int length) = 0;
    virtual void OnSentTo(AsyncDatagramSocket* socket, DWORD error,
                          int length, sockaddr* to, int to_length) = 0;
  };

  AsyncDatagramSocket();
  virtual ~AsyncDatagramSocket();

  virtual void Close();

  bool ReceiveAsync(void* buffer, int size, int flags, Listener* listener);
  OVERLAPPED* BeginReceive(void* buffer, int size, int flags, HANDLE event);
  int EndReceive(OVERLAPPED* overlapped);

  bool SendAsync(const void* buffer, int size, int flags, Listener* listener);
  OVERLAPPED* BeginSend(const void* buffer, int size, int flags, HANDLE event);
  int EndSend(OVERLAPPED* overlapped);

  bool ReceiveFromAsync(void* buffer, int size, int flags, Listener* listener);
  OVERLAPPED* BeginReceiveFrom(void* buffer, int size, int flags, HANDLE event);
  int EndReceiveFrom(OVERLAPPED* overlapped, sockaddr* address, int* length);

  bool SendToAsync(const void* buffer, int size, int flags,
                   const addrinfo* end_point, Listener* listener);
  OVERLAPPED* BeginSendTo(const void* buffer, int size, int flags,
                          const addrinfo* end_point, HANDLE event);
  int EndSendTo(OVERLAPPED* overlapped);

  operator HANDLE() const {
    return reinterpret_cast<HANDLE>(descriptor_);
  }

 private:
  enum Action {
    Receiving, Sending, ReceivingFrom, SendingTo
  };

  struct AsyncContext : OVERLAPPED, WSABUF {
    Action action;
    const addrinfo* end_point;
    DWORD flags;
    int address_length;
    sockaddr* address;
    AsyncDatagramSocket* socket;
    Listener* listener;
    HANDLE event;
    DWORD error;
  };

  AsyncContext* CreateAsyncContext();
  void DestroyAsyncContext(AsyncContext* context);

  static DWORD CALLBACK AsyncWork(void* param);
  static void CALLBACK OnTransferred(DWORD error, DWORD bytes,
                                     OVERLAPPED* overlapped);

  bool initialized_;
};

#endif  // JUNO_NET_ASYNC_DATAGRAM_SOCKET_H_
