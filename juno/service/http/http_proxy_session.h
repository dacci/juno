// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_HTTP_HTTP_PROXY_SESSION_H_
#define JUNO_SERVICE_HTTP_HTTP_PROXY_SESSION_H_

#include <stdint.h>

#include <base/atomic_ref_count.h>
#include <base/synchronization/condition_variable.h>
#include <base/synchronization/lock.h>

#include <memory>
#include <string>

#include "io/net/socket_channel.h"
#include "io/net/socket_resolver.h"
#include "misc/timer_service.h"
#include "service/service.h"

#include "service/http/http_request.h"
#include "service/http/http_response.h"

namespace juno {
namespace service {
namespace http {

struct HttpProxyConfig;

class HttpProxy;

class HttpProxySession : private io::Channel::Listener,
                         private io::net::SocketChannel::Listener,
                         private misc::TimerService::Callback {
 public:
  HttpProxySession(HttpProxy* proxy, const HttpProxyConfig* config,
                   const io::ChannelPtr& client);
  ~HttpProxySession();

  bool Start();
  void Stop();

 private:
  enum class State;

  static const size_t kBufferSize = 8 * 1024;  // 8 KiB
  static const int kTimeout = 15 * 1000;       // 15 sec

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

  void SetError(StatusCode status);
  void SendError(StatusCode status);
  void SendToRemote(const void* buffer, int length);

  void OnTimeout() override;

  void OnRead(io::Channel* channel, HRESULT result, void* buffer,
              int length) override;
  void OnWritten(io::Channel* channel, HRESULT result, void* buffer,
                 int length) override;

  void OnClosed(io::net::SocketChannel* socket, HRESULT result) override;

  void OnRequestReceived(HRESULT result, int length);
  void OnConnected(io::net::SocketChannel* socket, HRESULT result) override;
  void OnRequestSent(HRESULT result, int length);
  void OnRequestBodyReceived(HRESULT result, int length);
  void OnRequestBodySent(HRESULT result, int length);

  void OnResponseReceived(HRESULT result, int length);
  void OnResponseSent(HRESULT result, int length);
  void OnResponseBodyReceived(HRESULT result, int length);
  void OnResponseBodySent(HRESULT result, int length);

  HttpProxy* const proxy_;
  const HttpProxyConfig* config_;

  base::AtomicRefCount ref_count_;
  base::Lock lock_;
  base::ConditionVariable free_;

  misc::TimerService::TimerObject timer_;
  char buffer_[kBufferSize];
  State state_;
  int64_t last_chunk_size_;
  bool tunnel_;
  std::string last_host_;
  int last_port_;
  io::net::SocketResolver resolver_;
  bool retry_;
  int status_code_;

  io::ChannelPtr client_;
  std::string client_buffer_;
  HttpRequest request_;
  int64_t request_length_;
  bool request_chunked_;
  bool close_client_;

  std::shared_ptr<io::net::SocketChannel> remote_;
  std::string remote_buffer_;
  HttpResponse response_;
  int64_t response_length_;
  bool response_chunked_;
  bool close_remote_;

  HttpProxySession(const HttpProxySession&) = delete;
  HttpProxySession& operator=(const HttpProxySession&) = delete;
};

}  // namespace http
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_HTTP_HTTP_PROXY_SESSION_H_
