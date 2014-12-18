// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_HTTP_HTTP_PROXY_SESSION_H_
#define JUNO_SERVICE_HTTP_HTTP_PROXY_SESSION_H_

#include <madoka/concurrent/critical_section.h>
#include <madoka/net/resolver.h>
#include <madoka/net/socket_event_listener.h>

#include <memory>
#include <string>

#include "net/tunneling_service.h"
#include "service/service.h"
#include "service/http/http_request.h"
#include "service/http/http_response.h"

class HttpProxy;

class HttpProxySession
    : public madoka::net::SocketEventAdapter, public Channel::Listener {
 public:
  HttpProxySession(HttpProxy* proxy, const Service::ChannelPtr& client);
  ~HttpProxySession();

  bool Start();
  void Stop();

 private:
  enum State {
    Idle, Connecting, Header, Body,
  };

  enum Event {
    Connected, Received, Sent,
  };

  struct EventData {
    HttpProxySession* session;
    Event event;
    Channel* channel;
    DWORD error;
    void* buffer;
    int length;
  };

  static const size_t kBufferSize = 8 * 1024;   // 8 KiB
  static const int kTimeout = 15 * 1000;        // 15 sec

  static void CALLBACK TimerCallback(PTP_CALLBACK_INSTANCE instance,
                                     void* context, PTP_TIMER timer);
  void ClientReceiveAsync();

  bool FireEvent(Event event, Channel* channel, DWORD error, void* buffer,
                 int length);
  static void CALLBACK FireEvent(PTP_CALLBACK_INSTANCE instance, void* context);

  void ProcessRequest();
  void ProcessRequestChunk();
  void ProcessResponse();
  bool ProcessResponseChunk();

  void SendRequest();
  void SendResponse();
  void SendError(HTTP::StatusCode status);

  void EndRequest();
  bool EndResponse();

  void OnConnected(madoka::net::AsyncSocket* socket, DWORD error) override;
  void OnRead(Channel* channel, DWORD error, void* buffer, int length) override;
  void OnWritten(Channel* channel, DWORD error, void* buffer,
                 int length) override;

  void OnRequestReceived(int length);
  void OnRequestSent(DWORD error, int length);
  void OnRequestBodyReceived(int length);
  void OnRequestBodySent(DWORD error, int length);

  bool OnResponseReceived(DWORD error, int length);
  bool OnResponseSent(DWORD error, int length);
  bool OnResponseBodyReceived(DWORD error, int length);
  bool OnResponseBodySent(DWORD error, int length);

  static FILETIME kTimerDueTime;

  HttpProxy* const proxy_;
  madoka::concurrent::CriticalSection lock_;
  PTP_TIMER timer_;
  madoka::net::Resolver resolver_;
  bool send_error_;
  bool tunnel_;
  bool close_client_;
  std::string last_host_;
  int last_port_;

  Service::ChannelPtr client_;
  State client_state_;
  char client_buffer_[kBufferSize];
  std::string request_buffer_;
  HttpRequest request_;
  int64_t request_length_;
  bool request_chunked_;
  int64_t request_chunk_size_;
  int proxy_retry_;
  bool expect_continue_;

  AsyncSocketPtr remote_socket_;
  Service::ChannelPtr remote_;
  bool remote_connected_;
  State remote_state_;
  char remote_buffer_[kBufferSize];
  std::string response_buffer_;
  HttpResponse response_;
  int64_t response_length_;
  bool response_chunked_;
  int64_t response_chunk_size_;
};

#endif  // JUNO_SERVICE_HTTP_HTTP_PROXY_SESSION_H_
