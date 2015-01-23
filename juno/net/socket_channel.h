// Copyright (c) 2014 dacci.org

#ifndef JUNO_NET_SOCKET_CHANNEL_H_
#define JUNO_NET_SOCKET_CHANNEL_H_

#include <madoka/concurrent/condition_variable.h>
#include <madoka/concurrent/critical_section.h>
#include <madoka/net/async_socket.h>

#include <memory>
#include <vector>

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
  class Request;

  void EndRequest(Request* request);

  AsyncSocketPtr socket_;
  bool closed_;
  std::vector<std::unique_ptr<Request>> requests_;
  madoka::concurrent::CriticalSection lock_;
  madoka::concurrent::ConditionVariable empty_;
};

#endif  // JUNO_NET_SOCKET_CHANNEL_H_
