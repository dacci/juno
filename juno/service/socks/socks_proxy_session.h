// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SOCKS_SOCKS_PROXY_SESSION_H_
#define JUNO_SERVICE_SOCKS_SOCKS_PROXY_SESSION_H_

#include <memory>

#include "net/socket_channel.h"
#include "service/socks/socks4.h"
#include "service/socks/socks5.h"
#include "service/socks/socks_proxy.h"

class SocksProxySession : public SocketChannel::Listener,
                          public Channel::Listener {
 public:
  SocksProxySession(SocksProxy* proxy, const ChannelPtr& client);
  ~SocksProxySession();

  bool Start();
  void Stop();

  void OnConnected(SocketChannel* socket, HRESULT result) override;
  void OnRead(Channel* channel, HRESULT result, void* buffer,
              int length) override;
  void OnWritten(Channel* channel, HRESULT result, void* buffer,
                 int length) override;

  void OnClosed(SocketChannel* /*channel*/, HRESULT /*result*/) override {}

 private:
  static const size_t kBufferSize = 1024;

  bool ConnectIPv4(const SOCKS5::ADDRESS& address);
  bool ConnectDomain(const SOCKS5::ADDRESS& address);
  bool ConnectIPv6(const SOCKS5::ADDRESS& address);

  static void CALLBACK DeleteThis(PTP_CALLBACK_INSTANCE instance, void* param);

  SocksProxy* const proxy_;
  ChannelPtr client_;
  std::shared_ptr<SocketChannel> remote_;
  char request_buffer_[kBufferSize];
  char response_buffer_[kBufferSize];

  int phase_;
  void* end_point_;

  SocksProxySession(const SocksProxySession&) = delete;
  SocksProxySession& operator=(const SocksProxySession&) = delete;
};

#endif  // JUNO_SERVICE_SOCKS_SOCKS_PROXY_SESSION_H_
