// Copyright (c) 2016 dacci.org

#ifndef JUNO_SERVICE_SOCKS_SOCKS_SESSION_4_H_
#define JUNO_SERVICE_SOCKS_SOCKS_SESSION_4_H_

#include <string>

#include "net/socket_channel.h"

#include "service/socks/socks4.h"
#include "service/socks/socks_proxy.h"

class SocksSession4 : public SocksSession,
                      private Channel::Listener,
                      private SocketChannel::Listener {
 public:
  SocksSession4(SocksProxy* proxy, const ChannelPtr& channel);
  ~SocksSession4();

  HRESULT Start(std::unique_ptr<char[]>&& request, int length) override;
  void Stop() override;

 private:
  static const int kBufferSize = 1024;

  HRESULT ProcessRequest();
  HRESULT SendResponse(SOCKS4::CODE code);

  void OnRead(Channel* channel, HRESULT result, void* buffer,
              int length) override;
  void OnWritten(Channel* channel, HRESULT result, void* buffer,
                 int length) override;
  void OnConnected(SocketChannel* channel, HRESULT result) override;
  void OnClosed(SocketChannel* channel, HRESULT result) override;

  std::string message_;
  void* end_point_;
  std::shared_ptr<SocketChannel> remote_;

  SocksSession4(const SocksSession4&) = delete;
  SocksSession4& operator=(const SocksSession4&) = delete;
};

#endif  // JUNO_SERVICE_SOCKS_SOCKS_SESSION_4_H_
