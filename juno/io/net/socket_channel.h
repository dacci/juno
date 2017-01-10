// Copyright (c) 2016 dacci.org

#ifndef JUNO_IO_NET_SOCKET_CHANNEL_H_
#define JUNO_IO_NET_SOCKET_CHANNEL_H_

#include <base/synchronization/lock.h>

#include <memory>
#include <queue>

#include "io/channel.h"
#include "io/net/socket.h"

namespace juno {
namespace io {
namespace net {

class SocketChannel : public Socket, public Channel {
 public:
  class __declspec(novtable) Listener {
   public:
    virtual ~Listener() {}

    virtual void OnConnected(SocketChannel* channel, HRESULT result) = 0;
    virtual void OnClosed(SocketChannel* channel, HRESULT result) = 0;
  };

  SocketChannel();
  ~SocketChannel();

  void Close() override;
  HRESULT ReadAsync(void* buffer, int length,
                    Channel::Listener* listener) override;
  HRESULT WriteAsync(const void* buffer, int length,
                     Channel::Listener* listener) override;

  HRESULT ConnectAsync(const addrinfo* end_point, Listener* listener);
  HRESULT MonitorConnection(Listener* listener);

 private:
  struct Request;
  class Monitor;

  HRESULT DispatchRequest(std::unique_ptr<Request>&& request);

  static BOOL CALLBACK OnInitialize(INIT_ONCE* init_once, void* param,
                                    void** context);

  static void CALLBACK OnRequested(PTP_CALLBACK_INSTANCE callback,
                                   void* context, PTP_WORK work);
  void OnRequested(PTP_WORK work);

  static void CALLBACK OnCompleted(PTP_CALLBACK_INSTANCE callback,
                                   void* context, void* overlapped, ULONG error,
                                   ULONG_PTR bytes, PTP_IO io);
  void OnCompleted(OVERLAPPED* overlapped, ULONG error, ULONG_PTR bytes);

  static INIT_ONCE init_once_;

  base::Lock lock_;
  PTP_WORK work_;
  std::queue<std::unique_ptr<Request>> queue_;
  PTP_IO io_;
  bool abort_;

  SocketChannel(const SocketChannel&) = delete;
  SocketChannel& operator=(const SocketChannel&) = delete;
};

}  // namespace net
}  // namespace io
}  // namespace juno

#endif  // JUNO_IO_NET_SOCKET_CHANNEL_H_
