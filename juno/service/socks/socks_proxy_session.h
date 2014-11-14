// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SOCKS_SOCKS_PROXY_SESSION_H_
#define JUNO_SERVICE_SOCKS_SOCKS_PROXY_SESSION_H_

#include <madoka/net/async_socket.h>
#include <madoka/net/socket_event_listener.h>

#include <memory>

#include "service/socks/socks4.h"
#include "service/socks/socks5.h"
#include "service/socks/socks_proxy.h"

class SocksProxySession
    : public madoka::net::SocketEventAdapter, public Channel::Listener {
 public:
  explicit SocksProxySession(SocksProxy* proxy,
                             const Service::ChannelPtr& client);
  virtual ~SocksProxySession();

  bool Start();
  void Stop();

  void OnConnected(madoka::net::AsyncSocket* socket, DWORD error) override;
  void OnRead(Channel* channel, DWORD error, void* buffer, int length) override;
  void OnWritten(Channel* channel, DWORD error, void* buffer,
                 int length) override;

 private:
  typedef std::shared_ptr<madoka::net::AsyncSocket> AsyncSocketPtr;

  static const size_t kBufferSize = 1024;

  bool ConnectIPv4(const SOCKS5::ADDRESS& address);
  bool ConnectDomain(const SOCKS5::ADDRESS& address);
  bool ConnectIPv6(const SOCKS5::ADDRESS& address);

  static void CALLBACK DeleteThis(PTP_CALLBACK_INSTANCE instance, void* param);

  SocksProxy* const proxy_;
  Service::ChannelPtr client_;
  AsyncSocketPtr remote_;
  char request_buffer_[kBufferSize];
  char response_buffer_[kBufferSize];

  int phase_;
  void* end_point_;
};

#endif  // JUNO_SERVICE_SOCKS_SOCKS_PROXY_SESSION_H_
