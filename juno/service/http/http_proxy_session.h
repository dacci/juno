// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_HTTP_HTTP_PROXY_SESSION_H_
#define JUNO_SERVICE_HTTP_HTTP_PROXY_SESSION_H_

#include <madoka/concurrent/critical_section.h>
#include <madoka/net/resolver.h>
#include <madoka/net/socket_event_listener.h>

#include <memory>
#include <string>

#include "net/tunneling_service.h"
#include "service/http/http_request.h"
#include "service/http/http_response.h"

class HttpProxy;

class HttpProxySession : public madoka::net::SocketEventAdapter {
 public:
  HttpProxySession(HttpProxy* proxy, const AsyncSocketPtr& socket);
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
    madoka::net::AsyncSocket* socket;
    DWORD error;
    void* buffer;
    int length;
  };

  static const size_t kBufferSize = 8 * 1024;   // 8 KiB
  static const int kTimeout = 15 * 1000;        // 15 sec

  LONG AddRef();
  void Release();
  static void CALLBACK Delete(PTP_CALLBACK_INSTANCE instance, void* context);

  static void CALLBACK TimerCallback(PTP_CALLBACK_INSTANCE instance,
                                     void* context, PTP_TIMER timer);
  bool ClientReceiveAsync();

  bool FireEvent(Event event, madoka::net::AsyncSocket* socket, DWORD error,
                 void* buffer, int length);
  static void CALLBACK FireEvent(PTP_CALLBACK_INSTANCE instance, void* context);

  static int64_t ProcessMessageLength(HttpHeaders* headers);
  void ProcessHopByHopHeaders(HttpHeaders* headers);
  void ProcessRequest();
  bool ProcessRequestChunk();
  void ProcessResponse();
  bool ProcessResponseChunk();

  bool SendRequest();
  bool SendResponse();
  bool SendError(HTTP::StatusCode status);

  bool EndRequest();
  bool EndResponse();

  void OnConnected(madoka::net::AsyncSocket* socket, DWORD error) override;
  void OnReceived(madoka::net::AsyncSocket* socket, DWORD error, void* buffer,
                  int length) override;
  void OnSent(madoka::net::AsyncSocket* socket, DWORD error, void* buffer,
              int length) override;

  bool OnRequestReceived(int length);
  bool OnRequestSent(DWORD error, int length);
  bool OnRequestBodyReceived(int length);
  bool OnRequestBodySent(DWORD error, int length);

  bool OnResponseReceived(DWORD error, int length);
  bool OnResponseSent(DWORD error, int length);
  bool OnResponseBodyReceived(DWORD error, int length);
  bool OnResponseBodySent(DWORD error, int length);

  static FILETIME kTimerDueTime;

  HttpProxy* const proxy_;
  LONG ref_count_;
  madoka::concurrent::CriticalSection lock_;
  PTP_TIMER timer_;
  madoka::net::Resolver resolver_;
  bool send_error_;
  bool tunnel_;
  bool close_client_;
  std::string last_host_;
  int last_port_;

  AsyncSocketPtr client_;
  State client_state_;
  std::unique_ptr<char[]> client_buffer_;
  std::string request_buffer_;
  HttpRequest request_;
  int64_t request_length_;
  bool request_chunked_;
  int64_t request_chunk_size_;
  int proxy_retry_;
  bool expect_continue_;

  AsyncSocketPtr remote_;
  bool remote_connected_;
  State remote_state_;
  std::unique_ptr<char[]> remote_buffer_;
  std::string response_buffer_;
  HttpResponse response_;
  int64_t response_length_;
  bool response_chunked_;
  int64_t response_chunk_size_;
};

#endif  // JUNO_SERVICE_HTTP_HTTP_PROXY_SESSION_H_
