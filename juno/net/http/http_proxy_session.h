// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_HTTP_HTTP_PROXY_SESSION_H_
#define JUNO_NET_HTTP_HTTP_PROXY_SESSION_H_

#include <stdint.h>

#include <atlbase.h>
#include <atlutil.h>

#include "net/async_socket.h"
#include "net/http/http_request.h"
#include "net/http/http_response.h"
#include "net/http/http_status.h"

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
    Request, RequestBody, Response, ResponseBody, Error,
  };

  enum Event {
    Connected, Received, Sent,
  };

  struct EventArgs {
    Event event;
    HttpProxySession* session;
    AsyncSocket* socket;
    DWORD error;
    int length;
  };

  static const size_t kBufferSize = 4096;
  static const std::string kConnection;
  static const std::string kContentEncoding;
  static const std::string kContentLength;
  static const std::string kKeepAlive;
  static const std::string kProxyAuthenticate;
  static const std::string kProxyAuthorization;
  static const std::string kProxyConnection;
  static const std::string kTransferEncoding;

  static int64_t ParseChunk(const std::string& buffer, int64_t* chunk_size);

  void ProcessRequestHeader();
  void ProcessResponseHeader();
  void ProcessMessageLength(HttpHeaders* headers);
  void ProcessHopByHopHeaders(HttpHeaders* headers);

  void SendError(HTTP::StatusCode status);
  void EndSession();

  bool FireReceived(AsyncSocket* socket, DWORD error, int length);
  static DWORD CALLBACK FireEvent(void* param);

  void OnRequestReceived(AsyncSocket* socket, DWORD error, int length);
  void OnRequestSent(AsyncSocket* socket, DWORD error, int length);

  void OnRequestBodyReceived(AsyncSocket* socket, DWORD error, int length);
  void OnRequestBodySent(AsyncSocket* socket, DWORD error, int length);

  void OnResponseReceived(AsyncSocket* socket, DWORD error, int length);
  void OnResponseSent(AsyncSocket* socket, DWORD error, int length);

  void OnResponseBodyReceived(AsyncSocket* socket, DWORD error, int length);
  void OnResponseBodySent(AsyncSocket* socket, DWORD error, int length);

  AsyncSocket* client_;
  std::string client_buffer_;
  HttpRequest request_;
  bool tunnel_;
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
  bool close_client_;
};

#endif  // JUNO_NET_HTTP_HTTP_PROXY_SESSION_H_
