// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SOCKS_SOCKS_PROXY_SESSION_H_
#define JUNO_SERVICE_SOCKS_SOCKS_PROXY_SESSION_H_

#include <madoka/net/async_socket.h>
#include <madoka/net/socket_event_listener.h>

#include <memory>

#include "service/socks/socks4.h"
#include "service/socks/socks5.h"
#include "service/socks/socks_proxy.h"

class SocksProxySession : public madoka::net::SocketEventAdapter {
 public:
  explicit SocksProxySession(SocksProxy* proxy);
  virtual ~SocksProxySession();

  bool Start(const Service::AsyncSocketPtr& client);
  void Stop();

  void OnConnected(madoka::net::AsyncSocket* socket, DWORD error) override;
  void OnReceived(madoka::net::AsyncSocket* socket, DWORD error, void* buffer,
                  int length) override;
  void OnSent(madoka::net::AsyncSocket* socket, DWORD error, void* buffer,
              int length) override;

 private:
  static const size_t kBufferSize = 1024;

  bool ConnectIPv4(const SOCKS5::ADDRESS& address);
  bool ConnectDomain(const SOCKS5::ADDRESS& address);
  bool ConnectIPv6(const SOCKS5::ADDRESS& address);

  static void CALLBACK DeleteThis(PTP_CALLBACK_INSTANCE instance, void* param);

  SocksProxy* const proxy_;
  Service::AsyncSocketPtr client_;
  Service::AsyncSocketPtr remote_;
  std::unique_ptr<char[]> request_buffer_;
  std::unique_ptr<char[]> response_buffer_;

  int phase_;
  void* end_point_;
};

#endif  // JUNO_SERVICE_SOCKS_SOCKS_PROXY_SESSION_H_
