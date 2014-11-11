// Copyright (c) 2014 dacci.org

#ifndef JUNO_NET_SOCKET_CHANNEL_H_
#define JUNO_NET_SOCKET_CHANNEL_H_

#include <madoka/net/async_socket.h>
#include <madoka/net/socket_event_listener.h>

#include <memory>

#include "net/channel.h"

class SocketChannel : public Channel {
 public:
  explicit SocketChannel(madoka::net::AsyncSocket* socket);
  virtual ~SocketChannel();

  void Close() override;
  void ReadAsync(void* buffer, int length, Listener* listener) override;
  void WriteAsync(const void* buffer, int length, Listener* listener) override;

 private:
  class EventArgs : public madoka::net::SocketEventAdapter {
   public:
    void OnReceived(madoka::net::AsyncSocket* socket, DWORD error, void* buffer,
                    int length) override;
    void OnSent(madoka::net::AsyncSocket* socket, DWORD error, void* buffer,
                int length) override;

    SocketChannel* channel;
    Listener* listener;
    void* extra;
  };

  std::unique_ptr<madoka::net::AsyncSocket> socket_;
};

#endif  // JUNO_NET_SOCKET_CHANNEL_H_
