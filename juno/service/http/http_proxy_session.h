// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_HTTP_HTTP_PROXY_SESSION_H_
#define JUNO_SERVICE_HTTP_HTTP_PROXY_SESSION_H_

#include <stdint.h>

#include <madoka/concurrent/critical_section.h>
#include <madoka/net/resolver.h>
#include <madoka/net/socket_event_listener.h>

#include <memory>
#include <string>

#include "misc/timer_service.h"
#include "net/channel.h"
#include "service/service.h"
#include "service/http/http_request.h"
#include "service/http/http_response.h"

class HttpProxy;

class HttpProxySession
    : private Channel::Listener,
      private madoka::net::SocketEventAdapter,
      private TimerService::Callback {
 public:
  HttpProxySession(HttpProxy* proxy, const Service::ChannelPtr& client);
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

  void ReceiveResponse();
  void ProcessResponse();
  void ProcessResponseChunk();
  void SendResponse();
  void EndResponse();

  void SetError(HTTP::StatusCode status);
  void SendError(HTTP::StatusCode status);
  void SendToRemote(const void* buffer, int length);

  void OnTimeout() override;

  void OnRead(Channel* channel, DWORD error, void* buffer, int length) override;
  void OnWritten(Channel* channel, DWORD error, void* buffer,
                 int length) override;

  void OnReceived(madoka::net::AsyncSocket* socket, DWORD error, void* buffer,
                  int length) override;

  void OnRequestReceived(DWORD error, int length);
  void OnConnected(madoka::net::AsyncSocket* socket, DWORD error) override;
  void OnRequestSent(DWORD error, int length);
  void OnRequestBodyReceived(DWORD error, int length);
  void OnRequestBodySent(DWORD error, int length);

  void OnResponseReceived(DWORD error, int length);
  void OnResponseSent(DWORD error, int length);
  void OnResponseBodyReceived(DWORD error, int length);
  void OnResponseBodySent(DWORD error, int length);

  HttpProxy* const proxy_;
  madoka::concurrent::CriticalSection lock_;
  TimerService::TimerObject timer_;
  char buffer_[kBufferSize];
  State state_;
  int64_t last_chunk_size_;
  bool tunnel_;
  std::string last_host_;
  int last_port_;
  madoka::net::Resolver resolver_;
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
