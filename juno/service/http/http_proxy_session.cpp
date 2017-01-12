// Copyright (c) 2014 dacci.org

#include "service/http/http_proxy_session.h"

#include <base/logging.h>

#include <url/gurl.h>

#include <algorithm>
#include <string>

#include "io/net/socket_channel.h"
#include "misc/tunneling_service.h"

#include "service/http/http_proxy.h"
#include "service/http/http_proxy_config.h"
#include "service/http/http_util.h"

namespace juno {
namespace service {
namespace http {
namespace {

const std::string kConnection("Connection");
const std::string kContentLength("Content-Length");
const std::string kExpect("Expect");
const std::string kKeepAlive("Keep-Alive");
const std::string kProxyAuthenticate("Proxy-Authenticate");
const std::string kProxyAuthorization("Proxy-Authorization");

}  // namespace

enum class HttpProxySession::State {
  kIdle,
  kConnecting,
  kRequestHeader,
  kRequestBody,
  kResponseHeader,
  kResponseBody
};

HttpProxySession::HttpProxySession(HttpProxy* proxy,
                                   const HttpProxyConfig* config,
                                   const io::ChannelPtr& client)
    : proxy_(proxy),
      config_(config),
      ref_count_(0),
      free_(&lock_),
      timer_(misc::TimerService::GetDefault()->Create(this)),
      state_(State::kIdle),
      tunnel_(),
      last_port_(-1),
      retry_(),
      client_(client),
      close_remote_() {
  DLOG(INFO) << this << " session created";
}

HttpProxySession::~HttpProxySession() {
  if (timer_ != nullptr)
    timer_->Stop();

  remote_.reset();
  client_.reset();

  base::AutoLock guard(lock_);

  while (!base::AtomicRefCountIsZero(&ref_count_))
    free_.Wait();

  DLOG(INFO) << this << " session destroyed";
}

bool HttpProxySession::Start() {
  base::AutoLock guard(lock_);

  if (proxy_ == nullptr || config_ == nullptr || client_ == nullptr ||
      timer_ == nullptr)
    return false;

  DLOG(INFO) << this << " session started";

  EndResponse();

  return true;
}

void HttpProxySession::Stop() {
  base::AutoLock guard(lock_);

  DLOG(INFO) << this << " stop requested";

  if (timer_ != nullptr)
    timer_->Stop();

  if (remote_ != nullptr)
    remote_->Close();

  if (client_ != nullptr)
    client_->Close();
}

void HttpProxySession::ReceiveRequest() {
  if (timer_ == nullptr)
    return;

  timer_->Start(kTimeout, 0);
  auto result = client_->ReadAsync(buffer_, kBufferSize, this);
  if (FAILED(result)) {
    LOG(ERROR) << this << " failed to receive from client: 0x" << std::hex
               << result;
    proxy_->EndSession(this);
  }
}

// not safe to call from remote_'s event handler.
void HttpProxySession::ProcessRequest() {
  auto result = request_.Parse(client_buffer_);
  if (result > 0) {
    client_buffer_.erase(0, result);
  } else if (result == -2) {
    ReceiveRequest();
    return;
  } else {
    close_client_ = true;
    SendError(BAD_REQUEST);
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
      SendError(BAD_REQUEST);
      return;
    }

    tunnel_ = true;
  }

  if (http_util::ProcessHopByHopHeaders(&request_))
    close_client_ = true;

  config_->FilterHeaders(&request_, true);
  proxy_->ProcessAuthorization(&request_);

  DispatchRequest();
}

// not safe to call from remote_'s event handler.
void HttpProxySession::DispatchRequest() {
  GURL url;
  if (tunnel_) {
    url = GURL("http://" + request_.path());
  } else {
    url = GURL(request_.path());
    if (!url.is_valid() || !url.has_host()) {
      SetError(BAD_REQUEST);
      return;
    }

    if (!config_->use_remote_proxy()) {
      if (!url.SchemeIs("http")) {
        SetError(NOT_IMPLEMENTED);
        return;
      }

      request_.set_path(url.PathForRequest());
    }
  }

  int new_port;
  std::string new_host;
  if (config_->use_remote_proxy()) {
    new_host = config_->remote_proxy_host();
    new_port = config_->remote_proxy_port();
  } else {
    new_host = url.host();
    new_port = url.EffectiveIntPort();
  }

  if (new_port <= 0 || 65536 <= new_port) {
    SetError(BAD_REQUEST);
    return;
  }

  if (new_host == last_host_ && new_port == last_port_) {
    SendRequest();
    return;
  }

  last_host_ = std::move(new_host);
  last_port_ = new_port;

  auto result = resolver_.Resolve(last_host_, last_port_);
  if (FAILED(result)) {
    SetError(BAD_GATEWAY);
    return;
  }

  remote_ = std::make_shared<io::net::SocketChannel>();
  if (remote_ == nullptr) {
    SetError(INTERNAL_SERVER_ERROR);
    return;
  }

  state_ = State::kConnecting;
  remote_->ConnectAsync(resolver_.begin()->get(), this);
}

void HttpProxySession::SendRequest() {
  state_ = State::kRequestHeader;

  std::string message;
  request_.Serialize(&message);
  memmove(buffer_, message.data(), message.size());

  auto result =
      remote_->WriteAsync(buffer_, static_cast<int>(message.size()), this);
  if (FAILED(result)) {
    LOG(ERROR) << this << " failed to send to remote: 0x" << std::hex << result;
    SendError(INTERNAL_SERVER_ERROR);
  }
}

void HttpProxySession::ProcessRequestChunk() {
  request_length_ = http_util::ParseChunk(client_buffer_, &last_chunk_size_);
  if (request_length_ > 0) {
    SendToRemote(client_buffer_.data(), static_cast<int>(request_length_));
  } else if (request_length_ == -2) {
    ReceiveRequest();
  } else {
    LOG(ERROR) << this << " invalid request chunk";
    SetError(BAD_REQUEST);
  }
}

void HttpProxySession::EndRequest() {
  DLOG(INFO) << this << " request completed";

  if (status_code_ == 0) {
    state_ = State::kResponseHeader;

    remote_buffer_.clear();
    response_.Clear();
    response_length_ = 0;
    response_chunked_ = false;
    close_remote_ = false;

    auto result = ReceiveResponse();
    if (FAILED(result)) {
      LOG(ERROR) << this << " failed to receive response: 0x" << std::hex
                 << result;
      SendError(INTERNAL_SERVER_ERROR);
    }
  } else {
    SendError(static_cast<StatusCode>(status_code_));
  }
}

HRESULT HttpProxySession::ReceiveResponse() {
  return remote_->ReadAsync(buffer_, kBufferSize, this);
}

void HttpProxySession::ProcessResponse() {
  auto length = response_.Parse(remote_buffer_);
  if (length > 0) {
    remote_buffer_.erase(0, length);
  } else if (length == -2) {
    auto result = ReceiveResponse();
    if (FAILED(result)) {
      LOG(ERROR) << this << " failed to receive response: 0x" << std::hex
                 << result;
      SendError(INTERNAL_SERVER_ERROR);
    }
    return;
  } else {
    SendError(BAD_GATEWAY);
    return;
  }

  auto status = response_.status();

  DLOG(INFO) << this << " " << status << " " << response_.message();

  if (status / 100 == 1 || status == NO_CONTENT || status == NOT_MODIFIED ||
      request_.method().compare("HEAD") == 0) {
    response_length_ = 0;
  } else {
    response_length_ = http_util::GetContentLength(response_);
    if (response_length_ == -2) {
      response_chunked_ = true;
      response_length_ =
          http_util::ParseChunk(remote_buffer_, &last_chunk_size_);
    } else {
      response_chunked_ = false;
    }
  }

  if (tunnel_ && status == OK && (response_chunked_ || response_length_ > 0)) {
    SendError(BAD_GATEWAY);
    return;
  }

  if (http_util::ProcessHopByHopHeaders(&response_))
    close_remote_ = true;

  switch (status) {
    case PROXY_AUTHENTICATION_REQUIRED:
      retry_ = !retry_;
      break;

    default:
      retry_ = false;
      break;
  }

  if (retry_ && config_->use_remote_proxy() && config_->auth_remote_proxy()) {
    request_.RemoveHeader(kProxyAuthorization);
    proxy_->ProcessAuthenticate(&response_, &request_);

    if (response_chunked_ || response_length_ == -1 || response_length_ > 0) {
      state_ = State::kResponseBody;

      if (response_chunked_) {
        ProcessResponseChunk();
      } else {
        if (response_length_ > 0)
          response_length_ -= remote_buffer_.size();
        remote_buffer_.clear();

        if (response_length_ == -1 || response_length_ > 0) {
          auto result = ReceiveResponse();
          if (FAILED(result)) {
            LOG(ERROR) << this << " failed to receive response: 0x" << std::hex
                       << result;
            proxy_->EndSession(this);
          }
        } else {
          SendRequest();
        }
      }
    } else {
      SendRequest();
    }

    return;
  }

  if (tunnel_ && status == OK) {
    if (!misc::TunnelingService::Bind(client_, remote_)) {
      SendError(INTERNAL_SERVER_ERROR);
      return;
    }
  }

  if (!tunnel_) {
    config_->FilterHeaders(&response_, false);

    if (close_client_)
      response_.SetHeader(kConnection, "close");
  }

  SendResponse();
}

void HttpProxySession::ProcessResponseChunk() {
  response_length_ = http_util::ParseChunk(remote_buffer_, &last_chunk_size_);
  if (response_length_ > 0) {
    if (retry_) {
      OnResponseBodySent(0, static_cast<int>(response_length_));
    } else {
      auto result = client_->WriteAsync(
          remote_buffer_.data(), static_cast<int>(response_length_), this);
      if (FAILED(result)) {
        LOG(ERROR) << this << " failed to send to client: 0x" << std::hex
                   << result;
        proxy_->EndSession(this);
      }
    }
  } else if (response_length_ == -2) {
    auto result = ReceiveResponse();
    if (FAILED(result)) {
      LOG(ERROR) << this << " failed to receive response: 0x" << std::hex
                 << result;
      proxy_->EndSession(this);
    }
  } else {
    LOG(ERROR) << this << " invalid response chunk";
    proxy_->EndSession(this);
  }
}

void HttpProxySession::SendResponse() {
  state_ = State::kResponseHeader;

  std::string message;
  response_.Serialize(&message);
  memmove(buffer_, message.data(), message.size());

  auto result =
      client_->WriteAsync(buffer_, static_cast<int>(message.size()), this);
  if (FAILED(result)) {
    LOG(ERROR) << this << " failed to send to client: 0x" << std::hex << result;
    proxy_->EndSession(this);
  }
}

// not safe to call from remote_'s event handler.
void HttpProxySession::EndResponse() {
  DLOG(INFO) << this << " response completed";

  if (close_remote_) {
    if (remote_ != nullptr)
      remote_->Close();

    last_host_.clear();
    last_port_ = -1;
  }

  if (response_.status() / 100 == 1) {
    state_ = State::kResponseHeader;

    auto result = ReceiveResponse();
    if (FAILED(result)) {
      LOG(ERROR) << this << " failed to receive response: 0x" << std::hex
                 << result;
      SendError(INTERNAL_SERVER_ERROR);
    }

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

  state_ = State::kRequestHeader;
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

void HttpProxySession::SetError(StatusCode status) {
  DLOG(INFO) << this << " setting error: " << status;

  status_code_ = status;

  if (request_chunked_) {
    state_ = State::kRequestBody;

    if (request_length_ < 0) {  // bad chunk
      close_client_ = true;
      SendError(status);
    } else if (last_chunk_size_ == 0) {
      EndRequest();
    } else {
      ProcessRequestChunk();
    }
  } else if (request_length_ > 0) {
    state_ = State::kRequestBody;

    request_length_ -= client_buffer_.size();
    if (request_length_ > 0)
      ReceiveRequest();
    else
      EndRequest();
  } else {
    EndRequest();
  }
}

void HttpProxySession::SendError(StatusCode status) {
  DLOG(INFO) << this << " sending error: " << status;

  tunnel_ = false;
  retry_ = false;

  response_.Clear();
  response_length_ = 0;
  response_chunked_ = false;
  close_remote_ = true;

  response_.SetStatus(status);
  response_.SetHeader(kContentLength, "0");

  SendResponse();
}

void HttpProxySession::SendToRemote(const void* buffer, int length) {
  if (status_code_ != 0) {
    buffer = nullptr;
    length = 0;
  }

  auto result = remote_->WriteAsync(buffer, length, this);
  if (FAILED(result)) {
    LOG(ERROR) << this << " failed to send to remote: 0x" << std::hex << result;
    SendError(INTERNAL_SERVER_ERROR);
  }
}

void HttpProxySession::OnTimeout() {
  LOG(WARNING) << this << " request timed-out";
  Stop();
  proxy_->EndSession(this);
}

void HttpProxySession::OnRead(io::Channel* /*channel*/, HRESULT result,
                              void* /*buffer*/, int length) {
  base::AtomicRefCountInc(&ref_count_);

  base::AutoLock guard(lock_);

  switch (state_) {
    case State::kRequestHeader:
      OnRequestReceived(result, length);
      break;

    case State::kRequestBody:
      OnRequestBodyReceived(result, length);
      break;

    case State::kResponseHeader:
      OnResponseReceived(result, length);
      break;

    case State::kResponseBody:
      OnResponseBodyReceived(result, length);
      break;

    default:
      proxy_->EndSession(this);
      break;
  }

  if (!base::AtomicRefCountDec(&ref_count_))
    free_.Broadcast();
}

void HttpProxySession::OnWritten(io::Channel* /*channel*/, HRESULT result,
                                 void* /*buffer*/, int length) {
  base::AtomicRefCountInc(&ref_count_);

  base::AutoLock guard(lock_);

  switch (state_) {
    case State::kRequestHeader:
      OnRequestSent(result, length);
      break;

    case State::kRequestBody:
      OnRequestBodySent(result, length);
      break;

    case State::kResponseHeader:
      OnResponseSent(result, length);
      break;

    case State::kResponseBody:
      OnResponseBodySent(result, length);
      break;

    default:
      proxy_->EndSession(this);
      break;
  }

  if (!base::AtomicRefCountDec(&ref_count_))
    free_.Broadcast();
}

void HttpProxySession::OnClosed(io::net::SocketChannel* socket,
                                HRESULT /*result*/) {
  base::AtomicRefCountInc(&ref_count_);

  base::AutoLock guard(lock_);

  if (socket == remote_.get()) {
    LOG(WARNING) << this << " socket disconnected";

    last_host_.clear();
    last_port_ = -1;
  }

  if (!base::AtomicRefCountDec(&ref_count_))
    free_.Broadcast();
}

void HttpProxySession::OnRequestReceived(HRESULT result, int length) {
  timer_->Stop();

  if (FAILED(result) || length == 0) {
    LOG_IF(ERROR, FAILED(result)) << this << " failed to receive request: 0x"
                                  << std::hex << result;
    proxy_->EndSession(this);
    return;
  }

  client_buffer_.append(buffer_, length);
  ProcessRequest();
}

void HttpProxySession::OnConnected(io::net::SocketChannel* socket,
                                   HRESULT result) {
  base::AtomicRefCountInc(&ref_count_);

  base::AutoLock guard(lock_);

  if (SUCCEEDED(result)) {
    if (!tunnel_)
      socket->MonitorConnection(this);

    if (tunnel_ && !config_->use_remote_proxy()) {
      if (misc::TunnelingService::Bind(client_, remote_)) {
        response_.Clear();
        response_.SetStatus(OK, "Connection Established");
        response_.SetHeader(kContentLength, "0");
        SendResponse();
      } else {
        SendError(INTERNAL_SERVER_ERROR);
      }
    } else {
      SendRequest();
    }
  } else {
    LOG(ERROR) << this << " failed to connect: 0x" << std::hex << result;
    SendError(BAD_GATEWAY);
  }

  if (!base::AtomicRefCountDec(&ref_count_))
    free_.Broadcast();
}

void HttpProxySession::OnRequestSent(HRESULT result, int length) {
  if (FAILED(result) || length == 0) {
    LOG_IF(ERROR, FAILED(result)) << this << " failed to send request: 0x"
                                  << std::hex << result;
    SendError(BAD_GATEWAY);
    return;
  }

  if (request_chunked_ || request_length_ > 0) {
    state_ = State::kRequestBody;

    if (request_chunked_) {
      ProcessRequestChunk();
    } else if (client_buffer_.empty()) {
      ReceiveRequest();
    } else {
      auto size = std::min({client_buffer_.size(),
                            static_cast<size_t>(request_length_), kBufferSize});
      memmove(buffer_, client_buffer_.data(), size);
      client_buffer_.erase(0, size);

      SendToRemote(buffer_, static_cast<int>(size));
    }
  } else {
    EndRequest();
  }
}

void HttpProxySession::OnRequestBodyReceived(HRESULT result, int length) {
  timer_->Stop();

  if (FAILED(result) || length == 0) {
    LOG_IF(ERROR, FAILED(result))
        << this << " failed to receive request body: 0x" << std::hex << result;
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

void HttpProxySession::OnRequestBodySent(HRESULT result, int length) {
  if (FAILED(result) || length == 0) {
    LOG_IF(ERROR, FAILED(result)) << this << " failed to send request body: 0x"
                                  << std::hex << result;
    SendError(BAD_GATEWAY);
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

void HttpProxySession::OnResponseReceived(HRESULT result, int length) {
  if (FAILED(result) || length == 0) {
    LOG_IF(ERROR, FAILED(result)) << this << " failed to receive response: 0x"
                                  << std::hex << result;
    SendError(BAD_GATEWAY);
    return;
  }

  remote_buffer_.append(buffer_, length);
  ProcessResponse();
}

void HttpProxySession::OnResponseSent(HRESULT result, int length) {
  if (FAILED(result) || length == 0) {
    LOG_IF(ERROR, FAILED(result)) << this << " failed to send request: 0x"
                                  << std::hex << result;
    proxy_->EndSession(this);
    return;
  }

  if (response_length_ == 0 || tunnel_) {
    EndResponse();
    return;
  }

  state_ = State::kResponseBody;

  if (response_chunked_) {
    ProcessResponseChunk();
  } else if (remote_buffer_.empty()) {
    result = ReceiveResponse();
    if (FAILED(result)) {
      LOG(ERROR) << this << " failed to receive response: 0x" << std::hex
                 << result;
      proxy_->EndSession(this);
    }
  } else if (response_length_ < 0) {
    result = client_->WriteAsync(remote_buffer_.data(),
                                 static_cast<int>(remote_buffer_.size()), this);
    if (FAILED(result)) {
      LOG(ERROR) << this << " faield to send to client: 0x" << std::hex
                 << result;
      proxy_->EndSession(this);
    }
  } else {
    auto size = std::min({remote_buffer_.size(),
                          static_cast<size_t>(response_length_), kBufferSize});
    memmove(buffer_, remote_buffer_.data(), size);
    remote_buffer_.erase(0, size);

    result = client_->WriteAsync(buffer_, static_cast<int>(size), this);
    if (FAILED(result)) {
      LOG(ERROR) << this << " failed to send to client: 0x" << std::hex
                 << result;
      proxy_->EndSession(this);
    }
  }
}

void HttpProxySession::OnResponseBodyReceived(HRESULT result, int length) {
  if (FAILED(result) || length == 0) {
    LOG_IF(ERROR, FAILED(result))
        << this << " failed to receive response body: 0x" << std::hex << result;
    proxy_->EndSession(this);
    return;
  }

  if (response_chunked_) {
    remote_buffer_.append(buffer_, length);
    ProcessResponseChunk();
  } else {
    if (retry_) {
      OnResponseBodySent(0, length);
    } else {
      result = client_->WriteAsync(buffer_, length, this);
      if (FAILED(result)) {
        LOG(ERROR) << this << " failed to send to client: 0x" << std::hex
                   << result;
        proxy_->EndSession(this);
      }
    }
  }
}

void HttpProxySession::OnResponseBodySent(HRESULT result, int length) {
  if (FAILED(result) || length == 0) {
    LOG_IF(ERROR, FAILED(result)) << this << " failed to send response body: 0x"
                                  << std::hex << result;
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

    result = ReceiveResponse();
    if (FAILED(result)) {
      LOG(ERROR) << this << " failed to receive response: 0x" << std::hex
                 << result;
      proxy_->EndSession(this);
    }
  } else {
    EndResponse();
  }
}

}  // namespace http
}  // namespace service
}  // namespace juno
