// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_HTTP_HTTP_PROXY_SESSION_H_
#define JUNO_SERVICE_HTTP_HTTP_PROXY_SESSION_H_

#include <stdint.h>

#include <madoka/net/async_socket.h>

#include <base/synchronization/lock.h>

#include <memory>
#include <string>

#include "misc/timer_service.h"
#include "net/channel.h"
#include "net/socket_resolver.h"
#include "service/service.h"
#include "service/http/http_request.h"
#include "service/http/http_response.h"

class HttpProxy;
class HttpProxyConfig;

class HttpProxySession
    : private Channel::Listener,
      private madoka::net::AsyncSocket::Listener,
      private TimerService::Callback {
 public:
  HttpProxySession(HttpProxy* proxy,
                   const std::shared_ptr<HttpProxyConfig>& config,
                   const Service::ChannelPtr& client);
  ~HttpProxySession();

  bool Start();
  void Stop();

 private:
  enum State {
    Idle, Connecting, RequestHeader, RequestBody, ResponseHeader, ResponseBody
  };

  static const size_t kBufferSize = 8 * 1024;   // 8 KiB
  static const int kTimeout = 15 * 1000;        // 15 sec

  void ReceiveRequest();
  void ProcessRequest();
  void DispatchRequest();
  void SendRequest();
  void ProcessRequestChunk();
  // Called when all the request is sent to the remote server,
  // and begins receiving response.
  void EndRequest();

  HRESULT ReceiveResponse();
  void ProcessResponse();
  void ProcessResponseChunk();
  void SendResponse();
  void EndResponse();

  void SetError(HTTP::StatusCode status);
  void SendError(HTTP::StatusCode status);
  void SendToRemote(const void* buffer, int length);

  void OnTimeout() override;

  void OnRead(Channel* channel, HRESULT result, void* buffer,
              int length) override;
  void OnWritten(Channel* channel, HRESULT result, void* buffer,
                 int length) override;

  void OnReceived(madoka::net::AsyncSocket* socket, HRESULT result,
                  void* buffer, int length, int flags) override;

  void OnRequestReceived(HRESULT result, int length);
  void OnConnected(madoka::net::AsyncSocket* socket, HRESULT result,
                   const addrinfo* end_point) override;
  void OnRequestSent(HRESULT result, int length);
  void OnRequestBodyReceived(HRESULT result, int length);
  void OnRequestBodySent(HRESULT result, int length);

  void OnResponseReceived(HRESULT result, int length);
  void OnResponseSent(HRESULT result, int length);
  void OnResponseBodyReceived(HRESULT result, int length);
  void OnResponseBodySent(HRESULT result, int length);

  void OnReceivedFrom(madoka::net::AsyncSocket* socket, HRESULT result,
                      void* buffer, int length, int flags,
                      const sockaddr* address, int address_length) override {}
  void OnSent(madoka::net::AsyncSocket* socket, HRESULT result, void* buffer,
              int length) override {}
  void OnSentTo(madoka::net::AsyncSocket* socket, HRESULT result, void* buffer,
                int length, const sockaddr* address,
                int address_length) override {}

  HttpProxy* const proxy_;
  std::shared_ptr<HttpProxyConfig> config_;
  base::Lock lock_;
  TimerService::TimerObject timer_;
  char buffer_[kBufferSize];
  State state_;
  int64_t last_chunk_size_;
  bool tunnel_;
  std::string last_host_;
  int last_port_;
  SocketResolver resolver_;
  bool retry_;
  int status_code_;

  Service::ChannelPtr client_;
  std::string client_buffer_;
  HttpRequest request_;
  int64_t request_length_;
  bool request_chunked_;
  bool close_client_;

  madoka::net::AsyncSocket* remote_socket_;
  Service::ChannelPtr remote_;
  std::string remote_buffer_;
  HttpResponse response_;
  int64_t response_length_;
  bool response_chunked_;
  bool close_remote_;

  char peek_buffer_[16];
};

#endif  // JUNO_SERVICE_HTTP_HTTP_PROXY_SESSION_H_
