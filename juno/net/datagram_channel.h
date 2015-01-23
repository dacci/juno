// Copyright (c) 2014 dacci.org

#ifndef JUNO_NET_DATAGRAM_CHANNEL_H_
#define JUNO_NET_DATAGRAM_CHANNEL_H_

#include <madoka/concurrent/condition_variable.h>
#include <madoka/concurrent/critical_section.h>
#include <madoka/net/async_datagram_socket.h>

#include <memory>
#include <string>
#include <vector>

#include "net/channel.h"

class DatagramChannel : public Channel {
 public:
  typedef std::shared_ptr<madoka::net::AsyncDatagramSocket>
      AsyncDatagramSocketPtr;

  DatagramChannel();
  explicit DatagramChannel(const AsyncDatagramSocketPtr& socket);
  ~DatagramChannel();

  void Close() override;
  void ReadAsync(void* buffer, int length, Listener* listener) override;
  void WriteAsync(const void* buffer, int length, Listener* listener) override;

  bool Connect(const std::string& address, int port);
  int Read(void* buffer, int length);
  int Write(const void* buffer, int length);

 private:
  class Request;

  void EndRequest(Request* request);

  AsyncDatagramSocketPtr socket_;
  bool closed_;
  std::vector<std::unique_ptr<Request>> requests_;
  madoka::concurrent::CriticalSection lock_;
  madoka::concurrent::ConditionVariable empty_;
};

#endif  // JUNO_NET_DATAGRAM_CHANNEL_H_
