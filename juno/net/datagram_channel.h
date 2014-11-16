// Copyright (c) 2014 dacci.org

#ifndef JUNO_NET_DATAGRAM_CHANNEL_H_
#define JUNO_NET_DATAGRAM_CHANNEL_H_

#include <madoka/net/socket_event_listener.h>

#include <memory>

#include "net/channel.h"

class DatagramChannel : public Channel {
 public:
  typedef std::shared_ptr<madoka::net::AsyncDatagramSocket>
      AsyncDatagramSocketPtr;

  explicit DatagramChannel(const AsyncDatagramSocketPtr& socket);
  ~DatagramChannel();

  void Close() override;
  void ReadAsync(void* buffer, int length, Listener* listener) override;
  void WriteAsync(const void* buffer, int length, Listener* listener) override;

 private:
  class Request : public madoka::net::SocketEventAdapter {
   public:
    Request(DatagramChannel* channel, Listener* listener);

    void OnReceived(madoka::net::AsyncDatagramSocket* socket, DWORD error,
                    void* buffer, int length) override;
    void OnSent(madoka::net::AsyncDatagramSocket* socket, DWORD error,
                void* buffer, int length) override;

    DatagramChannel* const channel_;
    Listener* const listener_;
  };

  AsyncDatagramSocketPtr socket_;
};

#endif  // JUNO_NET_DATAGRAM_CHANNEL_H_
