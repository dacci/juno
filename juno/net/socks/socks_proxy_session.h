// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_SOCKS_SOCKS_PROXY_SESSION_H_
#define JUNO_NET_SOCKS_SOCKS_PROXY_SESSION_H_

#include <madoka/net/async_socket.h>
#include <madoka/net/socket_event_listener.h>

#include <memory>

#include "net/socks/socks4.h"
#include "net/socks/socks5.h"

class SocksProxy;

class SocksProxySession : public madoka::net::SocketEventAdapter {
 public:
  explicit SocksProxySession(SocksProxy* proxy);
  virtual ~SocksProxySession();

  bool Start(madoka::net::AsyncSocket* client);
  void Stop();

  void OnConnected(madoka::net::AsyncSocket* socket, DWORD error) override;
  void OnReceived(madoka::net::AsyncSocket* socket, DWORD error,
                  int length) override;
  void OnSent(madoka::net::AsyncSocket* socket, DWORD error,
              int length) override;

 private:
  typedef std::shared_ptr<madoka::net::AsyncSocket> AsyncSocketPtr;

  static const size_t kBufferSize = 1024;

  bool ConnectIPv4(const SOCKS5::ADDRESS& address);
  bool ConnectDomain(const SOCKS5::ADDRESS& address);
  bool ConnectIPv6(const SOCKS5::ADDRESS& address);

  static void CALLBACK DeleteThis(PTP_CALLBACK_INSTANCE instance, void* param);

  SocksProxy* const proxy_;
  AsyncSocketPtr client_;
  AsyncSocketPtr remote_;
  std::unique_ptr<char[]> request_buffer_;
  std::unique_ptr<char[]> response_buffer_;

  int phase_;
  void* end_point_;
};

#endif  // JUNO_NET_SOCKS_SOCKS_PROXY_SESSION_H_
