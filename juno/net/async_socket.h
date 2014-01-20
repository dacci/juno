// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_ASYNC_SOCKET_H_
#define JUNO_NET_ASYNC_SOCKET_H_

#include <madoka/net/socket.h>

#include <memory>

class AsyncSocket : public madoka::net::Socket {
 public:
  class Listener {
   public:
    virtual ~Listener() {}

    virtual void OnConnected(AsyncSocket* socket, DWORD error) = 0;
    virtual void OnReceived(AsyncSocket* socket, DWORD error, int length) = 0;
    virtual void OnSent(AsyncSocket* socket, DWORD error, int length) = 0;
  };

  AsyncSocket();
  virtual ~AsyncSocket();

  virtual void Close();

  bool UpdateAcceptContext(SOCKET descriptor);

  // Connects to a specified address asynchronously. The process will complete
  // when any connection attempt succeeds or all attempts fail.
  bool ConnectAsync(const addrinfo* end_points, Listener* listener);
  OVERLAPPED* BeginConnect(const addrinfo* end_points, HANDLE event);
  void EndConnect(OVERLAPPED* overlapped);

  void CancelAsyncConnect();

  bool ReceiveAsync(void* buffer, int size, int flags, Listener* listener);
  OVERLAPPED* BeginReceive(void* buffer, int size, int flags, HANDLE event);
  int EndReceive(OVERLAPPED* overlapped);

  bool SendAsync(const void* buffer, int size, int flags, Listener* listener);
  OVERLAPPED* BeginSend(const void* buffer, int size, int flags, HANDLE event);
  int EndSend(OVERLAPPED* overlapped);

  operator HANDLE() const {
    return reinterpret_cast<HANDLE>(descriptor_);
  }

 private:
  friend class AsyncServerSocket;

  enum Action {
    Connecting, Receiving, Sending
  };

  struct AsyncContext : OVERLAPPED, WSABUF {
    Action action;
    const addrinfo* end_point;
    DWORD flags;
    AsyncSocket* socket;
    Listener* listener;
    HANDLE event;
    DWORD error;
  };

  bool Init();

  AsyncContext* CreateAsyncContext();
  void DestroyAsyncContext(AsyncContext* context);

  int DoAsyncConnect(AsyncContext* context);

  static void CALLBACK AsyncWork(PTP_CALLBACK_INSTANCE instance, void* param);
  static void CALLBACK OnTransferred(PTP_CALLBACK_INSTANCE instance,
                                     void* self,
                                     void* overlapped,
                                     ULONG error,
                                     ULONG_PTR bytes,
                                     PTP_IO io);

  static LPFN_CONNECTEX ConnectEx;

  bool initialized_;
  PTP_IO io_;
  bool cancel_connect_;
};

typedef std::shared_ptr<AsyncSocket> AsyncSocketPtr;

#endif  // JUNO_NET_ASYNC_SOCKET_H_
