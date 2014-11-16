// Copyright (c) 2014 dacci.org

#ifndef JUNO_NET_SOCKET_CHANNEL_H_
#define JUNO_NET_SOCKET_CHANNEL_H_

#include <madoka/net/socket_event_listener.h>

#include <memory>

#include "net/channel.h"

class SocketChannel : public Channel {
 public:
  typedef std::shared_ptr<madoka::net::AsyncSocket> AsyncSocketPtr;

  explicit SocketChannel(const AsyncSocketPtr& socket);
  virtual ~SocketChannel();

  void Close() override;
  void ReadAsync(void* buffer, int length, Listener* listener) override;
  void WriteAsync(const void* buffer, int length, Listener* listener) override;

 private:
  class Request : public madoka::net::SocketEventAdapter {
   public:
    Request(SocketChannel* channel, Listener* listener);

    void OnReceived(madoka::net::AsyncSocket* socket, DWORD error, void* buffer,
                    int length) override;
    void OnSent(madoka::net::AsyncSocket* socket, DWORD error, void* buffer,
                int length) override;

    SocketChannel* const channel_;
    Listener* const listener_;
  };

  AsyncSocketPtr socket_;
};

#endif  // JUNO_NET_SOCKET_CHANNEL_H_
