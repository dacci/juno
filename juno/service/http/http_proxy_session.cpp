// Copyright (c) 2014 dacci.org

#include "service/http/http_proxy_session.h"

#include <assert.h>

#include <madoka/concurrent/lock_guard.h>
#include <madoka/net/async_socket.h>

#include <base/logging.h>

#include <url/gurl.h>

#include <algorithm>
#include <string>

#include "net/channel_util.h"
#include "net/socket_channel.h"
#include "net/tunneling_service.h"
#include "service/http/http_proxy.h"
#include "service/http/http_util.h"

#ifdef min
#undef min
#endif  // min

using ::madoka::net::AsyncSocket;

namespace {
const std::string kConnection("Connection");
const std::string kContentLength("Content-Length");
const std::string kExpect("Expect");
const std::string kKeepAlive("Keep-Alive");
const std::string kProxyAuthenticate("Proxy-Authenticate");
const std::string kProxyAuthorization("Proxy-Authorization");
}  // namespace

FILETIME HttpProxySession::kTimerDueTime = {
  -kTimeout * 1000 * 10,    // in 100-nanoseconds
  -1
};

HttpProxySession::HttpProxySession(HttpProxy* proxy,
                                   const Service::ChannelPtr& client)
    : proxy_(proxy),
      state_(Idle),
      tunnel_(),
      last_port_(-1),
      retry_(),
      client_(client),
      close_remote_() {
  timer_ = CreateThreadpoolTimer(TimerCallback, this, nullptr);
  DLOG(INFO) << this << " session created";
}

HttpProxySession::~HttpProxySession() {
  remote_.reset();
  client_.reset();

  madoka::concurrent::LockGuard guard(&lock_);

  SetThreadpoolTimer(timer_, nullptr, 0, 0);
  WaitForThreadpoolTimerCallbacks(timer_, TRUE);
  CloseThreadpoolTimer(timer_);
  timer_ = nullptr;

  DLOG(INFO) << this << " session destroyed";
}

bool HttpProxySession::Start() {
  madoka::concurrent::LockGuard guard(&lock_);

  if (proxy_ == nullptr || client_ == nullptr)
    return false;

  DLOG(INFO) << this << " session started";

  EndResponse();

  return true;
}

void HttpProxySession::Stop() {
  madoka::concurrent::LockGuard guard(&lock_);

  if (remote_ != nullptr)
    remote_->Close();

  client_->Close();

  DLOG(INFO) << this << " session stopped";
}

void HttpProxySession::ReceiveRequest() {
  if (timer_ != nullptr) {
    SetThreadpoolTimer(timer_, &kTimerDueTime, 0, 1000);
    client_->ReadAsync(buffer_, kBufferSize, this);
  }
}

void HttpProxySession::ProcessRequest() {
  int result = request_.Parse(client_buffer_);
  if (result > 0) {
    client_buffer_.erase(0, result);
  } else if (result == -2) {
    ReceiveRequest();
    return;
  } else {
    close_client_ = true;
    SendError(HTTP::BAD_REQUEST);
    return;
  }

  DLOG(INFO) << this << " " << request_.method() << " " << request_.path();

  request_length_ = http_util::GetContentLength(request_);
  if (request_length_ == -2) {
    request_chunked_ = true;
    request_length_ = http_util::ParseChunk(client_buffer_, &last_chunk_size_);
  } else {
    request_chunked_ = false;
  }

  if (request_.method().compare("CONNECT") == 0) {
    if (request_chunked_ || request_length_ > 0) {
      close_client_ = true;
      SendError(HTTP::BAD_REQUEST);
      return;
    } else {
      tunnel_ = true;
    }
  }

  if (http_util::ProcessHopByHopHeaders(&request_))
    close_client_ = true;

  proxy_->FilterHeaders(&request_, true);
  proxy_->ProcessAuthorization(&request_);

  DispatchRequest();
}

void HttpProxySession::DispatchRequest() {
  GURL url;
  if (tunnel_) {
    url = GURL("http://" + request_.path());
  } else {
    url = GURL(request_.path());
    if (!url.is_valid() || !url.has_host()) {
      SetError(HTTP::BAD_REQUEST);
      return;
    }

    if (!proxy_->use_remote_proxy()) {
      if (!url.SchemeIs("http")) {
        SetError(HTTP::NOT_IMPLEMENTED);
        return;
      }

      request_.set_path(url.PathForRequest());
    }
  }

  int new_port;
  std::string new_host;
  if (proxy_->use_remote_proxy()) {
    new_host = proxy_->remote_proxy_host();
    new_port = proxy_->remote_proxy_port();
  } else {
    new_host = url.host();
    new_port = url.EffectiveIntPort();
  }

  if (new_port <= 0 || 65536 <= new_port) {
    SetError(HTTP::BAD_REQUEST);
    return;
  }

  if (new_host == last_host_ && new_port == last_port_) {
    SendRequest();
    return;
  }

  if (remote_ != nullptr) {
    remote_->Close();

    lock_.Unlock();
    remote_.reset();
    lock_.Lock();
  }

  last_host_ = std::move(new_host);
  last_port_ = new_port;

  if (!resolver_.Resolve(last_host_.c_str(), last_port_)) {
    SetError(HTTP::BAD_GATEWAY);
    return;
  }

  auto remote_socket = std::make_shared<AsyncSocket>();
  if (remote_socket == nullptr) {
    SetError(HTTP::INTERNAL_SERVER_ERROR);
    return;
  }

  remote_ = std::make_shared<SocketChannel>(remote_socket);
  if (remote_ == nullptr) {
    SetError(HTTP::INTERNAL_SERVER_ERROR);
    return;
  }

  state_ = Connecting;
  remote_socket->ConnectAsync(*resolver_.begin(), this);
}

void HttpProxySession::SendRequest() {
  state_ = RequestHeader;

  std::string message;
  request_.Serialize(&message);
  memmove(buffer_, message.data(), message.size());

  remote_->WriteAsync(buffer_, message.size(), this);
}

void HttpProxySession::ProcessRequestChunk() {
  request_length_ = http_util::ParseChunk(client_buffer_, &last_chunk_size_);
  if (request_length_ > 0) {
    SendToRemote(client_buffer_.data(), request_length_);
  } else if (request_length_ == -2) {
    ReceiveRequest();
  } else {
    LOG(ERROR) << this << " invalid request chunk";
    SetError(HTTP::BAD_REQUEST);
  }
}

void HttpProxySession::EndRequest() {
  DLOG(INFO) << this << " request completed";

  if (status_code_ == 0) {
    state_ = ResponseHeader;

    remote_buffer_.clear();
    response_.Clear();
    response_length_ = 0;
    response_chunked_ = 0;
    close_remote_ = false;

    ReceiveResponse();
  } else {
    SendError(static_cast<HTTP::StatusCode>(status_code_));
  }
}

void HttpProxySession::ReceiveResponse() {
  remote_->ReadAsync(buffer_, kBufferSize, this);
}

void HttpProxySession::ProcessResponse() {
  int result = response_.Parse(remote_buffer_);
  if (result > 0) {
    remote_buffer_.erase(0, result);
  } else if (result == -2) {
    ReceiveResponse();
    return;
  } else {
    close_remote_ = true;
    SendError(HTTP::BAD_GATEWAY);
    return;
  }

  DLOG(INFO) << this << " " << response_.status() << " " << response_.message();

  response_length_ = http_util::GetContentLength(response_);
  if (response_length_ == -2) {
    response_chunked_ = true;
    response_length_ = http_util::ParseChunk(remote_buffer_, &last_chunk_size_);
  } else {
    response_chunked_ = false;
  }

  if (tunnel_ && response_.status() == HTTP::OK &&
      (response_chunked_ || response_length_ > 0)) {
    SendError(HTTP::BAD_GATEWAY);
    return;
  }

  if (http_util::ProcessHopByHopHeaders(&response_))
    close_remote_ = true;

  switch (response_.status()) {
    case HTTP::PROXY_AUTHENTICATION_REQUIRED:
      retry_ = !retry_;
      break;

    default:
      retry_ = false;
      break;
  }

  if (retry_ && proxy_->use_remote_proxy() && proxy_->auth_remote_proxy()) {
    request_.RemoveHeader(kProxyAuthorization);
    proxy_->ProcessAuthenticate(&response_, &request_);

    if (response_chunked_ || response_length_ == -1 || response_length_ > 0) {
      state_ = ResponseBody;

      if (response_chunked_) {
        ProcessResponseChunk();
      } else {
        if (response_length_ > 0)
          response_length_ -= remote_buffer_.size();
        remote_buffer_.clear();

        if (response_length_ == -1 || response_length_ > 0)
          ReceiveResponse();
        else
          SendRequest();
      }
    } else {
      SendRequest();
    }

    return;
  }

  if (tunnel_ && response_.status() == HTTP::OK) {
    if (!TunnelingService::Bind(client_, remote_)) {
      SendError(HTTP::INTERNAL_SERVER_ERROR);
      return;
    }
  }

  if (!tunnel_) {
    proxy_->FilterHeaders(&response_, false);

    if (close_client_)
      response_.SetHeader(kConnection, "close");
  }

  SendResponse();
}

void HttpProxySession::ProcessResponseChunk() {
  response_length_ = http_util::ParseChunk(remote_buffer_, &last_chunk_size_);
  if (response_length_ > 0) {
    if (retry_)
      OnResponseBodySent(0, response_length_);
    else
      client_->WriteAsync(remote_buffer_.data(), response_length_, this);
  } else if (response_length_ == -2) {
    ReceiveResponse();
  } else {
    LOG(ERROR) << this << " invalid response chunk";
    proxy_->EndSession(this);
  }
}

void HttpProxySession::SendResponse() {
  state_ = ResponseHeader;

  std::string message;
  response_.Serialize(&message);
  memmove(buffer_, message.data(), message.size());

  client_->WriteAsync(buffer_, message.size(), this);
}

void HttpProxySession::EndResponse() {
  DLOG(INFO) << this << " response completed";

  if (close_remote_) {
    if (remote_ != nullptr) {
      remote_->Close();

      lock_.Unlock();
      remote_.reset();
      lock_.Lock();
    }

    last_host_.clear();
    last_port_ = -1;
  }

  if (response_.status() / 100 == 1) {
    state_ = ResponseHeader;
    ReceiveResponse();
    return;
  }

  if (retry_) {
    DispatchRequest();
    return;
  }

  if (close_client_ || tunnel_) {
    proxy_->EndSession(this);
    return;
  }

  state_ = RequestHeader;
  last_chunk_size_ = 0;
  tunnel_ = false;
  retry_ = false;
  status_code_ = 0;

  request_.Clear();
  request_length_ = 0;
  request_chunked_ = false;
  close_client_ = false;

  if (client_buffer_.empty())
    ReceiveRequest();
  else
    ProcessRequest();
}

void HttpProxySession::SetError(HTTP::StatusCode status) {
  DLOG(INFO) << this << " setting error: " << status;

  status_code_ = status;

  if (request_chunked_) {
    state_ = RequestBody;

    if (request_length_ < 0) {  // bad chunk
      close_client_ = true;
      SendError(status);
    } else if (last_chunk_size_ == 0) {
      EndRequest();
    } else {
      ProcessRequestChunk();
    }
  } else if (request_length_ > 0) {
    state_ = RequestBody;

    request_length_ -= client_buffer_.size();
    if (request_length_ > 0)
      ReceiveRequest();
    else
      EndRequest();
  } else {
    EndRequest();
  }
}

void HttpProxySession::SendError(HTTP::StatusCode status) {
  DLOG(INFO) << this << " sending error: " << status;

  tunnel_ = false;
  retry_ = false;

  response_.Clear();
  response_length_ = 0;
  response_chunked_ = 0;
  close_remote_ = true;

  response_.SetStatus(status);
  response_.SetHeader(kContentLength, "0");

  SendResponse();
}

void HttpProxySession::SendToRemote(const void* buffer, int length) {
  if (status_code_ != 0)
    channel_util::FireEvent(this, ChannelEvent::Write, remote_.get(), 0, buffer,
                            length);
  else
    remote_->WriteAsync(buffer, length, this);
}

void CALLBACK HttpProxySession::TimerCallback(PTP_CALLBACK_INSTANCE instance,
                                              void* context, PTP_TIMER timer) {
  LOG(WARNING) << context << " request timed-out";

  SetThreadpoolTimer(timer, nullptr, 0, 0);
  static_cast<HttpProxySession*>(context)->client_->Close();
}

void HttpProxySession::OnRead(Channel* /*channel*/, DWORD error,
                              void* /*buffer*/, int length) {
  madoka::concurrent::LockGuard guard(&lock_);

  switch (state_) {
    case RequestHeader:
      OnRequestReceived(error, length);
      return;

    case RequestBody:
      OnRequestBodyReceived(error, length);
      return;

    case ResponseHeader:
      OnResponseReceived(error, length);
      return;

    case ResponseBody:
      OnResponseBodyReceived(error, length);
      return;

    default:
      proxy_->EndSession(this);
      return;
  }
}

void HttpProxySession::OnWritten(Channel* /*channel*/, DWORD error,
                                 void* /*buffer*/, int length) {
  madoka::concurrent::LockGuard guard(&lock_);

  switch (state_) {
    case RequestHeader:
      OnRequestSent(error, length);
      return;

    case RequestBody:
      OnRequestBodySent(error, length);
      return;

    case ResponseHeader:
      OnResponseSent(error, length);
      return;

    case ResponseBody:
      OnResponseBodySent(error, length);
      return;

    default:
      proxy_->EndSession(this);
      return;
  }
}

void HttpProxySession::OnReceived(AsyncSocket* socket, DWORD error,
                                  void* buffer, int length) {
  madoka::concurrent::LockGuard guard(&lock_);

  if (error != 0 || length == 0) {
    LOG(WARNING) << this << " socket disconnected";

    socket->Shutdown(SD_BOTH);
    last_host_.clear();
    last_port_ = -1;
  } else {
    socket->ReceiveAsync(peek_buffer_, sizeof(peek_buffer_), MSG_PEEK, this);
  }
}

void HttpProxySession::OnRequestReceived(DWORD error, int length) {
  if (timer_ != nullptr)
    SetThreadpoolTimer(timer_, nullptr, 0, 0);

  if (error != 0 || length == 0) {
    LOG_IF(ERROR, error != 0) << this
        << " failed to receive request: " << error;
    proxy_->EndSession(this);
    return;
  }

  client_buffer_.append(buffer_, length);
  ProcessRequest();
}

void HttpProxySession::OnConnected(AsyncSocket* socket, DWORD error) {
  madoka::concurrent::LockGuard guard(&lock_);

  if (error != 0) {
    LOG(ERROR) << this << " failed to connect: " << error;
    SendError(HTTP::BAD_GATEWAY);
    return;
  }

  if (!tunnel_)
    socket->ReceiveAsync(peek_buffer_, sizeof(peek_buffer_), MSG_PEEK, this);

  if (tunnel_ && !proxy_->use_remote_proxy()) {
    if (TunnelingService::Bind(client_, remote_)) {
      response_.Clear();
      response_.SetStatus(HTTP::OK, "Connection Established");
      response_.SetHeader(kContentLength, "0");
      SendResponse();
    } else {
      SendError(HTTP::INTERNAL_SERVER_ERROR);
    }
  } else {
    SendRequest();
  }
}

void HttpProxySession::OnRequestSent(DWORD error, int length) {
  if (error != 0 || length == 0) {
    LOG_IF(ERROR, error != 0) << " failed to send request: " << error;
    SendError(HTTP::BAD_GATEWAY);
    return;
  }

  if (request_chunked_ || request_length_ > 0) {
    state_ = RequestBody;

    if (request_chunked_) {
      ProcessRequestChunk();
    } else if (client_buffer_.empty()) {
      ReceiveRequest();
    } else {
      size_t size = std::min({ client_buffer_.size(),
                               static_cast<size_t>(request_length_),
                               kBufferSize });
      memmove(buffer_, client_buffer_.data(), size);
      client_buffer_.erase(0, size);

      SendToRemote(buffer_, size);
    }
  } else {
    EndRequest();
  }
}

void HttpProxySession::OnRequestBodyReceived(DWORD error, int length) {
  if (timer_ != nullptr)
    SetThreadpoolTimer(timer_, nullptr, 0, 0);

  if (error != 0 || length == 0) {
    LOG_IF(ERROR, error != 0) << this
        << " failed to receive request body: " << error;
    proxy_->EndSession(this);
    return;
  }

  if (request_chunked_) {
    client_buffer_.append(buffer_, length);
    ProcessRequestChunk();
  } else {
    SendToRemote(buffer_, length);
  }
}

void HttpProxySession::OnRequestBodySent(DWORD error, int length) {
  if (error != 0 || length == 0) {
    LOG_IF(ERROR, error != 0) << this
        << " failed to send request body: " << error;
    SendError(HTTP::BAD_GATEWAY);
    return;
  }

  if (request_chunked_) {
    client_buffer_.erase(0, length);

    if (last_chunk_size_ == 0)
      EndRequest();
    else
      ProcessRequestChunk();
  } else if (request_length_ > length) {
    request_length_ -= length;
    ReceiveRequest();
  } else {
    EndRequest();
  }
}

void HttpProxySession::OnResponseReceived(DWORD error, int length) {
  if (error != 0 || length == 0) {
    LOG_IF(ERROR, error != 0) << this
        << " failed to receive response: " << error;
    SendError(HTTP::BAD_GATEWAY);
    return;
  }

  remote_buffer_.append(buffer_, length);
  ProcessResponse();
}

void HttpProxySession::OnResponseSent(DWORD error, int length) {
  if (error != 0 || length == 0) {
    LOG_IF(ERROR, error != 0) << this << " failed to send request: " << error;
    proxy_->EndSession(this);
    return;
  }

  if (tunnel_) {
    if (remote_buffer_.empty()) {
      EndResponse();
    } else {
      state_ = Idle;
      client_->WriteAsync(remote_buffer_.data(), remote_buffer_.size(), this);
    }

    return;
  }

  if (request_.method().compare("HEAD") == 0 || response_length_ == 0) {
    EndResponse();
    return;
  }

  state_ = ResponseBody;

  if (response_chunked_) {
    ProcessResponseChunk();
  } else if (remote_buffer_.empty()) {
    ReceiveResponse();
  } else if (response_length_ < 0) {
    client_->WriteAsync(remote_buffer_.data(), remote_buffer_.size(), this);
  } else {
    size_t size = std::min({ remote_buffer_.size(),
                             static_cast<size_t>(response_length_),
                             kBufferSize });
    memmove(buffer_, remote_buffer_.data(), size);
    remote_buffer_.erase(0, size);

    client_->WriteAsync(buffer_, size, this);
  }
}

void HttpProxySession::OnResponseBodyReceived(DWORD error, int length) {
  if (error != 0 || length == 0) {
    LOG_IF(ERROR, error != 0) << this
        << " failed to receive response body: " << error;
    proxy_->EndSession(this);
    return;
  }

  if (response_chunked_) {
    remote_buffer_.append(buffer_, length);
    ProcessResponseChunk();
  } else {
    if (retry_)
      OnResponseBodySent(0, length);
    else
      client_->WriteAsync(buffer_, length, this);
  }
}

void HttpProxySession::OnResponseBodySent(DWORD error, int length) {
  if (error != 0 || length == 0) {
    LOG_IF(ERROR, error != 0) << this
        << " failed to send response body: " << error;
    proxy_->EndSession(this);
    return;
  }

  if (response_chunked_) {
    remote_buffer_.erase(0, length);

    if (last_chunk_size_ == 0)
      EndResponse();
    else
      ProcessResponseChunk();
  } else if (response_length_ == -1 || response_length_ > length) {
    if (response_length_ > length)
      response_length_ -= length;

    ReceiveResponse();
  } else {
    EndResponse();
  }
}
