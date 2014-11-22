// Copyright (c) 2014 dacci.org

#include "service/http/http_proxy_session.h"

#include <assert.h>

#include <madoka/concurrent/lock_guard.h>
#include <madoka/net/async_socket.h>

#include <url/gurl.h>

#include <algorithm>
#include <string>

#include "misc/string_util.h"
#include "net/socket_channel.h"
#include "service/http/http_proxy.h"
#include "service/http/http_util.h"

#ifdef _DEBUG
#define ASSERT_FALSE assert(false)
#else
#define ASSERT_FALSE __assume(0)
#endif  // _DEBUG

#ifdef min
#undef min
#endif

using ::madoka::net::AsyncSocket;

namespace {
const std::string kConnection("Connection");
const std::string kContentLength("Content-Length");
const std::string kExpect("Expect");
const std::string kKeepAlive("Keep-Alive");
const std::string kProxyAuthenticate("Proxy-Authenticate");
const std::string kProxyAuthorization("Proxy-Authorization");
const std::string kProxyConnection("Proxy-Connection");
const std::string kTransferEncoding("Transfer-Encoding");
}  // namespace

FILETIME HttpProxySession::kTimerDueTime = {
  -kTimeout * 1000 * 10,    // in 100-nanoseconds
  -1
};

HttpProxySession::HttpProxySession(HttpProxy* proxy,
                                   const Service::ChannelPtr& client)
    : proxy_(proxy),
      close_client_(false),
      send_error_(false),
      last_port_(-1),
      client_(client),
      client_state_(Idle),
      request_length_(0),
      request_chunked_(false),
      request_chunk_size_(0),
      proxy_retry_(0),
      expect_continue_(false),
      remote_socket_(new AsyncSocket()),
      remote_(new SocketChannel(remote_socket_)),
      remote_connected_(false),
      remote_state_(Idle),
      response_length_(0),
      response_chunked_(false),
      response_chunk_size_(0) {
  timer_ = ::CreateThreadpoolTimer(TimerCallback, this, nullptr);
}

HttpProxySession::~HttpProxySession() {
  Stop();

  madoka::concurrent::LockGuard guard(&lock_);

  if (timer_ != nullptr) {
    ::SetThreadpoolTimer(timer_, nullptr, 0, 0);
    ::WaitForThreadpoolTimerCallbacks(timer_, TRUE);
    ::CloseThreadpoolTimer(timer_);
    timer_ = nullptr;
  }
}

bool HttpProxySession::Start() {
  if (remote_socket_ == nullptr || remote_ == nullptr)
    return false;

  ClientReceiveAsync();

  return true;
}

void HttpProxySession::Stop() {
  if (remote_socket_ != nullptr) {
    if (remote_socket_->connected())
      remote_->Close();
    else if (remote_state_ == Connecting)
      remote_socket_->Close();
  }

  if (client_ != nullptr)
    client_->Close();

  madoka::concurrent::LockGuard guard(&lock_);
}

void CALLBACK HttpProxySession::TimerCallback(PTP_CALLBACK_INSTANCE instance,
                                              void* context, PTP_TIMER timer) {
  static_cast<HttpProxySession*>(context)->client_->Close();
}

void HttpProxySession::ClientReceiveAsync() {
  ::SetThreadpoolTimer(timer_, &kTimerDueTime, 0, 0);
  client_->ReadAsync(client_buffer_, kBufferSize, this);
}

bool HttpProxySession::FireEvent(Event event, Channel* channel, DWORD error,
                                 void* buffer, int length) {
  std::unique_ptr<EventData> data(new EventData());
  if (data == nullptr)
    return false;

  data->session = this;
  data->event = event;
  data->channel = channel;
  data->error = error;
  data->buffer = buffer;
  data->length = length;

  if (::TrySubmitThreadpoolCallback(FireEvent, data.get(), nullptr)) {
    data.release();
    return true;
  }

  return false;
}

void CALLBACK HttpProxySession::FireEvent(PTP_CALLBACK_INSTANCE instance,
                                          void* context) {
  std::unique_ptr<EventData> data(static_cast<EventData*>(context));

  switch (data->event) {
    case Received:
      data->session->OnRead(data->channel, data->error, data->buffer,
                            data->length);
      break;

    case Sent:
      data->session->OnWritten(data->channel, data->error, data->buffer,
                               data->length);
      break;

    default:
      ASSERT_FALSE;
  }
}

int64_t HttpProxySession::ProcessMessageLength(HttpHeaders* headers) {
  // RFC 2616 - 4.4 Message Length
  if (headers->HeaderExists(kTransferEncoding) &&
      ::_stricmp(headers->GetHeader(kTransferEncoding), "identity") != 0)
    return -1;
  else if (headers->HeaderExists(kContentLength))
    return std::stoll(headers->GetHeader(kContentLength));
  else
    return -2;
}

void HttpProxySession::ProcessHopByHopHeaders(HttpHeaders* headers) {
  if (headers->HeaderExists(kConnection)) {
    const std::string& connection = headers->GetHeader(kConnection);
    size_t start = 0;

    while (true) {
      size_t end = connection.find_first_of(',', start);
      size_t length = std::string::npos;
      if (end != std::string::npos)
        length = end - start;

      std::string token = connection.substr(start, length);
      if (::_stricmp(token.c_str(), "close") == 0)
        close_client_ = true;
      else
        headers->RemoveHeader(token);

      if (end == std::string::npos)
        break;

      start = connection.find_first_not_of(" \t", end + 1);
    }

    headers->RemoveHeader(kConnection);
  }

  // remove other hop-by-hop headers
  headers->RemoveHeader(kKeepAlive);
  // Let the client process unsupported proxy auth methods.
  // headers->RemoveHeader(kProxyAuthenticate);
  // headers->RemoveHeader(kProxyAuthorization);
  headers->RemoveHeader(kProxyConnection);
}

void HttpProxySession::ProcessRequest() {
  request_length_ = ProcessMessageLength(&request_);
  request_chunked_ = request_length_ == -1;

  if (request_.minor_version() < 1)
    close_client_ = true;

  ProcessHopByHopHeaders(&request_);

  if (request_.minor_version() == 0)
    request_.AddHeader(kConnection, kKeepAlive);

  proxy_->FilterHeaders(&request_, true);
  proxy_->ProcessAuthorization(&request_);
}

void HttpProxySession::ProcessRequestChunk() {
  request_length_ = http_util::ParseChunk(request_buffer_,
                                          &request_chunk_size_);

  if (request_length_ == -2)
    ClientReceiveAsync();
  else if (request_length_ > 0)
    remote_->WriteAsync(request_buffer_.data(), request_length_, this);
  else
    SendError(HTTP::BAD_REQUEST);
}

void HttpProxySession::ProcessResponse() {
  response_length_ = ProcessMessageLength(&response_);
  response_chunked_ = response_length_ == -1;

  if (response_.minor_version() < 1)
    close_client_ = true;

  ProcessHopByHopHeaders(&response_);

  proxy_->FilterHeaders(&response_, false);

  if (close_client_)
    response_.AddHeader(kConnection, "close");
}

bool HttpProxySession::ProcessResponseChunk() {
  while (true) {
    response_length_ = http_util::ParseChunk(response_buffer_,
                                             &response_chunk_size_);

    if (response_length_ == -2) {
      remote_->ReadAsync(remote_buffer_, kBufferSize, this);
      return true;
    } else if (response_length_ > 0) {
      if (proxy_retry_ != 1) {
        client_->WriteAsync(response_buffer_.data(), response_length_, this);
        return true;
      }

      response_buffer_.erase(0, response_length_);

      if (response_chunk_size_ == 0)
        return EndResponse();
    } else {
      return false;
    }
  }
}

void HttpProxySession::SendRequest() {
  std::string message;
  request_.Serialize(&message);
  ::memmove(remote_buffer_, message.data(), message.size());

  client_state_ = Header;

  remote_->WriteAsync(remote_buffer_, message.size(), this);
}

void HttpProxySession::SendResponse() {
  std::string message;
  response_.Serialize(&message);
  ::memmove(client_buffer_, message.data(), message.size());

  remote_state_ = Header;

  client_->WriteAsync(client_buffer_, message.size(), this);
}

void HttpProxySession::SendError(HTTP::StatusCode status) {
  if (remote_state_ != Idle) {
    send_error_ = true;
    remote_->Close();
  }

  tunnel_ = false;

  response_.Clear();
  response_.set_minor_version(1);
  response_.SetStatus(status);
  response_.SetHeader(kContentLength, "0");

  request_length_ = 0;

  if (client_state_ != Idle)
    EndRequest();

  SendResponse();
}

void HttpProxySession::EndRequest() {
  if (proxy_retry_ == 1) {
    client_state_ = Idle;
    return;
  }

  if (expect_continue_)
    client_state_ = Body;
  else
    client_state_ = Idle;

  if (request_buffer_.empty())
    client_->ReadAsync(nullptr, 0, this);
}

bool HttpProxySession::EndResponse() {
  if (close_client_ && proxy_retry_ != 1) {
    proxy_->EndSession(this);
    return true;
  }

  remote_state_ = Idle;

  if (!remote_connected_)
    return true;

  remote_->ReadAsync(nullptr, 0, this);

  if (proxy_retry_ == 1) {
    SendRequest();
    return true;
  }

  proxy_retry_ = 0;

  assert(request_buffer_.size() <= kBufferSize);

  if (request_buffer_.empty()) {
    ::SetThreadpoolTimer(timer_, &kTimerDueTime, 0, 0);
    return true;
  } else if (request_buffer_.size() <= kBufferSize) {
    size_t size = request_buffer_.size();
    ::memmove(client_buffer_, request_buffer_.data(), size);
    request_buffer_.erase();

    return FireEvent(Received, client_.get(), 0, client_buffer_, size);
  }

  return false;
}

void HttpProxySession::OnConnected(AsyncSocket* socket, DWORD error) {
  madoka::concurrent::LockGuard guard(&lock_);

  remote_state_ = Idle;

  if (error != 0) {
    tunnel_ = false;
    SendError(HTTP::BAD_GATEWAY);
    return;
  }

  response_buffer_.clear();
  response_.Clear();

  if (tunnel_ && !proxy_->use_remote_proxy()) {
    tunnel_ = TunnelingService::Bind(client_, remote_);
    if (tunnel_) {
      remote_.reset();
      remote_socket_.reset();

      response_.set_minor_version(1);
      response_.SetStatus(HTTP::OK, "Connection Established");
      SendResponse();
    } else {
      SendError(HTTP::INTERNAL_SERVER_ERROR);
    }

    return;
  }

  if (!remote_connected_) {
    remote_->ReadAsync(nullptr, 0, this);
    remote_connected_ = true;
  }

  SendRequest();
}

void HttpProxySession::OnRead(Channel* channel, DWORD error, void* buffer,
                              int length) {
  madoka::concurrent::LockGuard guard(&lock_);

  if (error == 0 && length == 0 && buffer != nullptr)
    error = WSAEDISCON;

  if (error != 0)
    channel->Close();

  if (channel == client_.get()) {
    ::SetThreadpoolTimer(timer_, nullptr, 0, 0);

    if (buffer == nullptr && length == 0) {
      ClientReceiveAsync();
      return;
    }

    if (error == 0) {
      switch (client_state_) {
        case Idle:
        case Header:
          OnRequestReceived(length);
          break;

        case Body:
          OnRequestBodyReceived(length);
          break;

        default:
          ASSERT_FALSE;
      }
    } else {
      proxy_->EndSession(this);
    }
  } else if (channel == remote_.get()) {
    if (buffer == nullptr && length == 0) {
      remote_->ReadAsync(remote_buffer_, kBufferSize, this);
      return;
    }

    if (error == 0) {
      switch (remote_state_) {
        case Idle:
        case Header:
          if (!OnResponseReceived(error, length))
            error = __LINE__;
          break;

        case Body:
          if (!OnResponseBodyReceived(error, length))
            error = __LINE__;
          break;

        default:
          ASSERT_FALSE;
      }
    }

    if (error != 0) {
      remote_connected_ = false;

      if (send_error_)
        send_error_ = false;
      else if (client_state_ == Idle)
        client_->Close();
    }
  }
}

void HttpProxySession::OnWritten(Channel* channel, DWORD error, void* buffer,
                                 int length) {
  madoka::concurrent::LockGuard guard(&lock_);

  if (error == 0 && length == 0 && buffer != nullptr)
    error = WSAEDISCON;

  if (error != 0)
    channel->Close();

  if (channel == remote_.get()) {
    if (error == 0) {
      switch (client_state_) {
        case Header:
          OnRequestSent(error, length);
          break;

        case Body:
          OnRequestBodySent(error, length);
          break;

        default:
          ASSERT_FALSE;
      }
    } else {
      remote_connected_ = false;
      SendError(HTTP::BAD_GATEWAY);
    }
  } else if (channel == client_.get()) {
    if (error == 0) {
      switch (remote_state_) {
        case Header:
          if (!OnResponseSent(error, length))
            error = __LINE__;
          break;

        case Body:
          if (!OnResponseBodySent(error, length))
            error = __LINE__;
          break;

        default:
          ASSERT_FALSE;
      }
    }

    if (error != 0)
      proxy_->EndSession(this);
  }
}

void HttpProxySession::OnRequestReceived(int length) {
  client_state_ = Header;
  request_buffer_.append(client_buffer_, length);

  int result = request_.Parse(request_buffer_);
  if (result > 0) {
    request_buffer_.erase(0, result);
  } else if (result == HttpRequest::kPartial) {
    ClientReceiveAsync();
    return;
  } else if (result == HttpRequest::kError) {
    request_buffer_.clear();
    SendError(HTTP::BAD_REQUEST);
    return;
  } else {
    ASSERT_FALSE;
  }

  ProcessRequest();

  expect_continue_ = false;
  if (request_.HeaderExists(kExpect)) {
    expect_continue_ = request_.GetHeader(kExpect).compare("100-continue") == 0;
    if (!expect_continue_) {
      SendError(HTTP::EXPECTATION_FAILED);
      return;
    }
  }

  tunnel_ = request_.method().compare("CONNECT") == 0;

  GURL url;
  if (tunnel_) {
    url = GURL("http://" + request_.path());
    if (url.EffectiveIntPort() == url::PORT_UNSPECIFIED) {
      SendError(HTTP::BAD_REQUEST);
      return;
    }
  } else {
    url = GURL(request_.path());
    if (!url.is_valid()) {
      SendError(HTTP::BAD_REQUEST);
      return;
    }

    // I can handle only HTTP, for now.
    if (!proxy_->use_remote_proxy() && !url.SchemeIs("http")) {
      SendError(HTTP::NOT_IMPLEMENTED);
      return;
    }

    if (!proxy_->use_remote_proxy())
      request_.set_path(url.PathForRequest());
  }

  std::string new_host;
  int new_port;
  if (proxy_->use_remote_proxy()) {
    new_host = proxy_->remote_proxy_host();
    new_port = proxy_->remote_proxy_port();
  } else {
    new_host = url.host();
    new_port = url.EffectiveIntPort();
  }

  if (remote_socket_->connected() &&
      last_host_ == new_host && last_port_ == new_port) {
    SendRequest();
    return;
  }

  last_host_ = std::move(new_host);
  last_port_ = new_port;

  if (!resolver_.Resolve(last_host_.c_str(), new_port)) {
    SendError(HTTP::BAD_GATEWAY);
    return;
  }

  lock_.Unlock();
  remote_socket_->Close();
  lock_.Lock();

  remote_state_ = Connecting;
  remote_socket_->ConnectAsync(*resolver_.begin(), this);
}

void HttpProxySession::OnRequestSent(DWORD error, int length) {
  client_state_ = Body;

  if (tunnel_) {
    client_state_ = Idle;
  } else if (request_chunked_) {
    ProcessRequestChunk();
  } else if (request_length_ > 0) {
    if (request_buffer_.empty()) {
      ClientReceiveAsync();
    } else {
      size_t size = std::min(request_buffer_.size(),
                             static_cast<size_t>(request_length_));
      ::memmove(client_buffer_, request_buffer_.data(), size);
      request_buffer_.erase(0, size);

      remote_->WriteAsync(client_buffer_, size, this);
    }
  } else {
    EndRequest();
  }
}

void HttpProxySession::OnRequestBodyReceived(int length) {
  if (request_chunked_) {
    request_buffer_.append(client_buffer_, length);
    ProcessRequestChunk();
  } else {
    remote_->WriteAsync(client_buffer_, length, this);
  }
}

void HttpProxySession::OnRequestBodySent(DWORD error, int length) {
  if (request_chunked_) {
    request_buffer_.erase(0, length);

    if (request_chunk_size_ == 0)
      EndRequest();
    else
      ProcessRequestChunk();
  } else if (request_length_ > length) {
    request_length_ -= length;
    ClientReceiveAsync();
  } else {
    EndRequest();
  }
}

bool HttpProxySession::OnResponseReceived(DWORD error, int length) {
  remote_state_ = Header;
  response_buffer_.append(remote_buffer_, length);

  int result = response_.Parse(response_buffer_);
  if (result > 0) {
    response_buffer_.erase(0, result);
  } else if (result == HttpResponse::kPartial) {
    remote_->ReadAsync(remote_buffer_, kBufferSize, this);
    return true;
  } else if (result == HttpResponse::kError) {
    response_buffer_.clear();
    return false;
  } else {
    ASSERT_FALSE;
  }

  ProcessResponse();

  if (response_.status() == HTTP::PROXY_AUTHENTICATION_REQUIRED)
    ++proxy_retry_;
  else
    proxy_retry_ = 0;

  if (proxy_retry_ == 1 &&
      proxy_->use_remote_proxy() && proxy_->auth_remote_proxy()) {
    request_.RemoveHeader(kProxyAuthorization);
    proxy_->ProcessAuthenticate(&response_, &request_);

    remote_state_ = Body;

    if (response_chunked_ || response_length_ > 0) {
      if (response_buffer_.empty()) {
        remote_->ReadAsync(remote_buffer_, kBufferSize, this);
        return true;
      } else {
        int length = response_buffer_.size();
        ::memmove(remote_buffer_, response_buffer_.data(), length);
        response_buffer_.clear();
        return OnResponseBodyReceived(0, length);
      }
    } else {
      return EndResponse();
    }
  }

  if (tunnel_) {
    tunnel_ = response_.status() == HTTP::OK;
    if (tunnel_) {
      tunnel_ = TunnelingService::Bind(client_, remote_);
      if (tunnel_) {
        remote_.reset();
        remote_socket_.reset();
      } else {
        EndRequest();
        SendError(HTTP::INTERNAL_SERVER_ERROR);
        return true;
      }
    } else {
      EndRequest();
    }
  }

  SendResponse();
  return true;
}

bool HttpProxySession::OnResponseSent(DWORD error, int length) {
  remote_state_ = Body;

  if (tunnel_) {
    client_.reset();
    proxy_->EndSession(this);
    return true;
  } else if (response_chunked_) {
    return ProcessResponseChunk();
  } else if (response_length_ > 0 || response_length_ == -2) {
    if (response_buffer_.empty()) {
      remote_->ReadAsync(remote_buffer_, kBufferSize, this);
      return true;
    } else {
      size_t size = response_buffer_.size();
      if (response_length_ != -2 && size > response_length_)
        size = response_length_;

      ::memmove(remote_buffer_, response_buffer_.data(), size);
      response_buffer_.erase(0, size);

      client_->WriteAsync(remote_buffer_, size, this);
      return true;
    }
  } else {
    return EndResponse();
  }
}

bool HttpProxySession::OnResponseBodyReceived(DWORD error, int length) {
  if (response_chunked_) {
    response_buffer_.append(remote_buffer_, length);

    return ProcessResponseChunk();
  } else {
    if (proxy_retry_ == 1) {
      return OnResponseBodySent(error, length);
    } else {
      client_->WriteAsync(remote_buffer_, length, this);
      return true;
    }
  }
}

bool HttpProxySession::OnResponseBodySent(DWORD error, int length) {
  if (response_chunked_) {
    response_buffer_.erase(0, length);

    if (response_chunk_size_ == 0)
      return EndResponse();
    else
      return ProcessResponseChunk();
  } else if (response_length_ > length || response_length_ == -2) {
    if (response_length_ > length)
      response_length_ -= length;

    remote_->ReadAsync(remote_buffer_, kBufferSize, this);
    return true;
  } else {
    return EndResponse();
  }
}
