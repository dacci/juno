// Copyright (c) 2016 dacci.org

#ifndef JUNO_NET_SOCKET_CHANNEL_H_
#define JUNO_NET_SOCKET_CHANNEL_H_

#include <base/synchronization/lock.h>

#include <list>
#include <memory>

#include "net/channel.h"
#include "net/socket.h"

class SocketChannel : public Socket, public Channel {
 public:
  class __declspec(novtable) Listener {
   public:
    virtual ~Listener() {}

    virtual void OnConnected(SocketChannel* channel, HRESULT result) = 0;
    virtual void OnClosed(SocketChannel* channel, HRESULT result) = 0;
  };

  SocketChannel();
  virtual ~SocketChannel();

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

  static BOOL CALLBACK OnInitialize(INIT_ONCE* init_once, void* param,
                                    void** context);

  static void CALLBACK OnRequested(PTP_CALLBACK_INSTANCE callback,
                                   void* context, PTP_WORK work);
  void OnRequested(PTP_WORK work);

  static void CALLBACK OnCompleted(PTP_CALLBACK_INSTANCE callback,
                                   void* context, void* overlapped, ULONG error,
                                   ULONG_PTR bytes, PTP_IO io);

  static INIT_ONCE init_once_;

  base::Lock lock_;
  PTP_WORK work_;
  std::list<std::unique_ptr<Request>> queue_;
  PTP_IO io_;
  bool abort_;

  SocketChannel(const SocketChannel&) = delete;
  SocketChannel& operator=(const SocketChannel&) = delete;
};

#endif  // JUNO_NET_SOCKET_CHANNEL_H_
