// Copyright (c) 2016 dacci.org

#ifndef JUNO_IO_NET_ASYNC_SERVER_SOCKET_H_
#define JUNO_IO_NET_ASYNC_SERVER_SOCKET_H_

#include <base/synchronization/lock.h>

#include <memory>
#include <queue>
#include <type_traits>

#include "io/net/server_socket.h"

namespace juno {
namespace io {
namespace net {

class AsyncServerSocket : public ServerSocket {
 public:
  class Context;

  class __declspec(novtable) Listener {
   public:
    virtual ~Listener() {}

    virtual void OnAccepted(AsyncServerSocket* server, HRESULT result,
                            Context* context) = 0;
  };

  AsyncServerSocket();
  ~AsyncServerSocket();

  void Close() override;

  HRESULT AcceptAsync(Listener* listener);

  template <class T>
  std::unique_ptr<T> EndAccept(Context* context, HRESULT* result) {
    static_assert(std::is_base_of<Socket, T>::value,
                  "T must be a descendant of Socket");

    struct Wrapper : T {
      explicit Wrapper(SOCKET descriptor) {
        descriptor_ = descriptor;
        bound_ = true;
        connected_ = true;
      }
    };

    auto peer_descriptor = EndAcceptImpl(context, result);
    if (peer_descriptor == INVALID_SOCKET)
      return nullptr;

    auto peer = std::make_unique<Wrapper>(peer_descriptor);
    if (peer == nullptr) {
      if (result != nullptr)
        *result = E_OUTOFMEMORY;

      closesocket(peer_descriptor);
      return nullptr;
    }

    if (result != nullptr)
      *result = S_OK;

    return std::move(peer);
  }

 private:
  static const DWORD kAddressBufferSize = sizeof(sockaddr_storage) + 16;

  HRESULT DispatchRequest(std::unique_ptr<Context>&& request);
  SOCKET EndAcceptImpl(Context* context, HRESULT* result);

  static void CALLBACK OnRequested(PTP_CALLBACK_INSTANCE callback,
                                   void* context, PTP_WORK work);
  void OnRequested(PTP_WORK work);

  static void CALLBACK OnCompleted(PTP_CALLBACK_INSTANCE callback,
                                   void* context, void* overlapped, ULONG error,
                                   ULONG_PTR bytes, PTP_IO io);
  void OnCompleted(OVERLAPPED* overlapped, ULONG error, ULONG_PTR bytes);

  base::Lock lock_;
  PTP_WORK work_;
  std::queue<std::unique_ptr<Context>> queue_;
  PTP_IO io_;
  WSAPROTOCOL_INFO protocol_;

  AsyncServerSocket(const AsyncServerSocket&) = delete;
  AsyncServerSocket& operator=(const AsyncServerSocket&) = delete;
};

}  // namespace net
}  // namespace io
}  // namespace juno

#endif  // JUNO_IO_NET_ASYNC_SERVER_SOCKET_H_
