// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_SOCKS_SOCKS_PROXY_SESSION_H_
#define JUNO_NET_SOCKS_SOCKS_PROXY_SESSION_H_

#include "net/async_socket.h"
#include "net/socks/socks4.h"
#include "net/socks/socks5.h"

class SocksProxy;

class SocksProxySession : public AsyncSocket::Listener {
 public:
  explicit SocksProxySession(SocksProxy* proxy);
  virtual ~SocksProxySession();

  bool Start(AsyncSocket* client);
  void Stop();

  void OnConnected(AsyncSocket* socket, DWORD error);
  void OnReceived(AsyncSocket* socket, DWORD error, int length);
  void OnSent(AsyncSocket* socket, DWORD error, int length);

 private:
  static const size_t kBufferSize = 1024;

  bool ConnectIPv4(const SOCKS5::ADDRESS& address);
  bool ConnectDomain(const SOCKS5::ADDRESS& address);
  bool ConnectIPv6(const SOCKS5::ADDRESS& address);

  static void CALLBACK DeleteThis(PTP_CALLBACK_INSTANCE instance, void* param);

  SocksProxy* const proxy_;
  AsyncSocketPtr client_;
  AsyncSocketPtr remote_;
  char* request_buffer_;
  char* response_buffer_;

  int phase_;
  addrinfo* end_point_;
};

#endif  // JUNO_NET_SOCKS_SOCKS_PROXY_SESSION_H_
