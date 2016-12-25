// Copyright (c) 2016 dacci.org

#ifndef JUNO_SERVICE_SOCKS_SOCKS_SESSION_5_H_
#define JUNO_SERVICE_SOCKS_SOCKS_SESSION_5_H_

#include "service/socks/socks_proxy.h"

#include <string>

#include "io/net/socket_channel.h"

class SocksSession5 : public SocksSession,
                      private Channel::Listener,
                      private SocketChannel::Listener {
 public:
  SocksSession5(SocksProxy* proxy, const ChannelPtr& channel);
  ~SocksSession5();

  HRESULT Start(std::unique_ptr<char[]>&& request, int length) override;
  void Stop() override;

 private:
  static const int kBufferSize = 1024;

  enum class State {
    Init,
    Negotiation,
    Command,
  };

  HRESULT ReadRequest();
  HRESULT EnsureRead(size_t min_length);

  HRESULT ProcessRequest();
  HRESULT ProcessNegotiation();
  HRESULT ProcessCommand();

  void OnRead(Channel* channel, HRESULT result, void* buffer,
              int length) override;
  void OnWritten(Channel* channel, HRESULT result, void* buffer,
                 int length) override;
  void OnConnected(SocketChannel* channel, HRESULT result) override;
  void OnClosed(SocketChannel* channel, HRESULT result) override;

  State state_;
  std::string message_;
  void* end_point_;
  std::shared_ptr<SocketChannel> remote_;

  SocksSession5(const SocksSession5&) = delete;
  SocksSession5& operator=(const SocksSession5&) = delete;
};

#endif  // JUNO_SERVICE_SOCKS_SOCKS_SESSION_5_H_
