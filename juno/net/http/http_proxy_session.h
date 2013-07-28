// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_HTTP_HTTP_PROXY_SESSION_H_
#define JUNO_NET_HTTP_HTTP_PROXY_SESSION_H_

#include <stdint.h>

#include <atlbase.h>
#include <atlutil.h>

#include "net/async_socket.h"
#include "net/http/http_request.h"
#include "net/http/http_response.h"

class HttpProxySession : public AsyncSocket::Listener {
 public:
  explicit HttpProxySession(AsyncSocket* client);
  virtual ~HttpProxySession();

  bool Start();

  void OnConnected(AsyncSocket* socket, DWORD error);
  void OnReceived(AsyncSocket* socket, DWORD error, int length);
  void OnSent(AsyncSocket* socket, DWORD error, int length);

 private:
  enum Phase {
    Request, RequestBody, Response, ResponseBody,
  };

  static const size_t kBufferSize = 4096;

  static int64_t ParseChunk(const std::string& buffer, int64_t* chunk_size);

  AsyncSocket* client_;
  std::string client_buffer_;
  HttpRequest request_;
  CUrl url_;
  madoka::net::AddressInfo resolver_;

  AsyncSocket* remote_;
  std::string remote_buffer_;
  HttpResponse response_;

  char* buffer_;
  Phase phase_;
  int64_t content_length_;
  bool chunked_;
  int64_t chunk_size_;
};

#endif  // JUNO_NET_HTTP_HTTP_PROXY_SESSION_H_
