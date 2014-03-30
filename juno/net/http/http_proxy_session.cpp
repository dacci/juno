// Copyright (c) 2013 dacci.org

#include "net/http/http_proxy_session.h"

#include <assert.h>

#include "misc/string_util.h"
#include "net/http/http_proxy.h"
#include "net/http/http_proxy_config.h"
#include "net/http/http_util.h"
#include "net/tunneling_service.h"

#define DELETE_THIS() \
  ::TrySubmitThreadpoolCallback(DeleteThis, this, NULL)

const std::string HttpProxySession::kConnection("Connection");
const std::string HttpProxySession::kContentEncoding("Content-Encoding");
const std::string HttpProxySession::kContentLength("Content-Length");
const std::string HttpProxySession::kExpect("Expect");
const std::string HttpProxySession::kKeepAlive("Keep-Alive");
const std::string HttpProxySession::kProxyAuthenticate("Proxy-Authenticate");
const std::string HttpProxySession::kProxyAuthorization("Proxy-Authorization");
const std::string HttpProxySession::kProxyConnection("Proxy-Connection");
const std::string HttpProxySession::kTransferEncoding("Transfer-Encoding");

FILETIME HttpProxySession::kTimerDueTime = {
  -kTimeout * 1000 * 10,   // in 100-nanoseconds
  -1
};

HttpProxySession::HttpProxySession(HttpProxy* proxy, HttpProxyConfig* config)
    : proxy_(proxy),
      config_(config),
      client_(),
      remote_(),
      buffer_(new char[kBufferSize]),
      phase_(Request),
      close_client_(),
      timer_(),
      continue_() {
  timer_ = ::CreateThreadpoolTimer(OnTimeout, this, NULL);
}

HttpProxySession::~HttpProxySession() {
  Stop();

  if (timer_ != NULL) {
    ::SetThreadpoolTimer(timer_, NULL, 0, 0);
    ::WaitForThreadpoolTimerCallbacks(timer_, TRUE);
    ::CloseThreadpoolTimer(timer_);
    timer_ = NULL;
  }

  proxy_->EndSession(this);
}

bool HttpProxySession::Start(AsyncSocket* client) {
  client_.reset(client);

  if (!ReceiveAsync(client_, 0)) {
    client_ = NULL;
    return false;
  }

  return true;
}

void HttpProxySession::Stop() {
  if (client_ != NULL)
    client_->Shutdown(SD_BOTH);

  if (remote_ != NULL) {
    if (remote_->connected())
      remote_->Shutdown(SD_BOTH);
    else
      remote_->CancelAsyncConnect();
  }
}

void HttpProxySession::OnConnected(AsyncSocket* socket, DWORD error) {
  if (error != 0) {
    SendError(HTTP::BAD_GATEWAY);
    return;
  }

  if (tunnel_ && !config_->use_remote_proxy()) {
    phase_ = Response;

    if (!TunnelingService::Bind(client_, remote_)) {
      SendError(HTTP::INTERNAL_SERVER_ERROR);
      return;
    }

    remote_buffer_ += "HTTP/1.1 200 Connection Established\x0D\x0A";
    remote_buffer_ += "Content-Length: 0\x0D\x0A";
    remote_buffer_ += "\x0D\x0A";

    response_.Parse(remote_buffer_);

    if (!client_->SendAsync(remote_buffer_.data(), remote_buffer_.size(), 0,
                            this)) {
      DELETE_THIS();
      return;
    }
  } else {
    remote_buffer_ += request_.method();
    remote_buffer_ += ' ';

    if (config_->use_remote_proxy())
      remote_buffer_ += request_.path();
    else
      remote_buffer_ += url_.PathForRequest();

    remote_buffer_ += " HTTP/1.";
    remote_buffer_ += '0' + request_.minor_version();
    remote_buffer_ += "\x0D\x0A";
    request_.SerializeHeaders(&remote_buffer_);
    remote_buffer_ += "\x0D\x0A";

    if (!remote_->SendAsync(remote_buffer_.data(), remote_buffer_.size(), 0,
                            this)) {
      SendError(HTTP::INTERNAL_SERVER_ERROR);
      return;
    }
  }
}

void HttpProxySession::OnReceived(AsyncSocket* socket, DWORD error,
                                  int length) {
  assert(timer_ != NULL);
  ::SetThreadpoolTimer(timer_, NULL, 0, 0);

  switch (phase_) {
    case Request:
      OnRequestReceived(error, length);
      return;

    case RequestBody:
      OnRequestBodyReceived(error, length);
      return;

    case Response:
      OnResponseReceived(error, length);
      return;

    case ResponseBody:
      OnResponseBodyReceived(error, length);
      return;
  }

  assert(false);
}

void HttpProxySession::OnSent(AsyncSocket* socket, DWORD error, int length) {
  switch (phase_) {
    case Request:
      OnRequestSent(error, length);
      return;

    case RequestBody:
      OnRequestBodySent(error, length);
      return;

    case Response:
      OnResponseSent(error, length);
      return;

    case ResponseBody:
      OnResponseBodySent(error, length);
      return;

    case Error:
      DELETE_THIS();
      return;
  }

  assert(false);
}

bool HttpProxySession::ReceiveAsync(const AsyncSocketPtr& socket, int flags) {
  assert(timer_ != NULL);
  ::SetThreadpoolTimer(timer_, &kTimerDueTime, 0, 0);

  receiving_ = socket.get();

  return socket->ReceiveAsync(buffer_.get(), kBufferSize, flags, this);
}

void HttpProxySession::ProcessRequestHeader() {
  content_length_ = 0;
  chunked_ = false;
  ProcessMessageLength(&request_);
  if (request_.minor_version() < 1)
    close_client_ = true;

  ProcessHopByHopHeaders(&request_);

  proxy_->FilterHeaders(&request_, true);

  if (config_->use_remote_proxy() && config_->auth_remote_proxy())
    request_.AddHeader(kProxyAuthorization, config_->basic_authorization());
}

void HttpProxySession::ProcessResponseHeader() {
  content_length_ = -2;
  chunked_ = false;
  ProcessMessageLength(&response_);
  if (response_.minor_version() < 1)
    close_client_ = true;

  ProcessHopByHopHeaders(&response_);

  proxy_->FilterHeaders(&response_, false);

  if (close_client_)
    response_.AddHeader(kConnection, "close");
}

void HttpProxySession::ProcessMessageLength(HttpHeaders* headers) {
  // RFC 2616 - 4.4 Message Length
  if (headers->HeaderExists(kTransferEncoding) &&
      ::_stricmp(headers->GetHeader(kTransferEncoding), "identity") != 0) {
    chunked_ = true;
  } else if (headers->HeaderExists(kContentLength)) {
    content_length_ = std::stoll(headers->GetHeader(kContentLength));
  }
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

void HttpProxySession::SendError(HTTP::StatusCode status) {
  phase_ = Response;

  const char* message = HTTP::GetStatusMessage(status);
  if (message == NULL)
    message = HTTP::GetStatusMessage(HTTP::INTERNAL_SERVER_ERROR);

  int length = ::sprintf_s(buffer_.get(), kBufferSize,
                           "HTTP/1.1 %d %s\x0D\x0A"
                           "Content-Length: 0\x0D\x0A"
                           "\x0D\x0A", status, message);
  if (length > 0) {
    response_.Parse(buffer_.get(), length);

    if (client_->SendAsync(buffer_.get(), length, 0, this))
      return;
  }

  DELETE_THIS();
}

// Resets internal stats and starts new session.
void HttpProxySession::EndSession() {
  if (close_client_) {
    DELETE_THIS();
    return;
  }

  if (remote_ != NULL)
    remote_->Shutdown(SD_BOTH);

  request_.Clear();
  url_ = GURL::EmptyGURL();

  remote_buffer_.clear();
  response_.Clear();

  phase_ = Request;
  close_client_ = false;
  continue_ = false;

  if (client_buffer_.empty()) {
    if (!ReceiveAsync(client_, 0))
      DELETE_THIS();
  } else {
    int length = client_buffer_.size();
    ::memmove(buffer_.get(), client_buffer_.data(), length);
    client_buffer_.clear();

    FireReceived(client_, 0, length);
  }
}

bool HttpProxySession::FireReceived(const AsyncSocketPtr& socket, DWORD error,
                                    int length) {
  EventArgs* args = new EventArgs;
  if (args == NULL)
    return false;

  ::memset(args, 0, sizeof(*args));

  args->event = Received;
  args->session = this;
  args->socket = socket.get();
  args->error = error;
  args->length = length;

  if (!::TrySubmitThreadpoolCallback(FireEvent, args, NULL)) {
    delete args;
    return false;
  }

  return true;
}

void CALLBACK HttpProxySession::FireEvent(PTP_CALLBACK_INSTANCE instance,
                                          void* param) {
  EventArgs* args = static_cast<EventArgs*>(param);

  switch (args->event) {
    case Connected:
      args->session->OnConnected(args->socket, args->error);
      break;

    case Received:
      args->session->OnReceived(args->socket, args->error, args->length);
      break;

    case Sent:
      args->session->OnSent(args->socket, args->error, args->length);
      break;
  }

  delete args;
}

void HttpProxySession::OnRequestReceived(DWORD error, int length) {
  if (error != 0 || length == 0) {
    DELETE_THIS();
    return;
  }

  client_buffer_.append(buffer_.get(), length);

  int result = request_.Parse(client_buffer_);
  if (result == HttpRequest::kPartial) {
    if (!ReceiveAsync(client_, 0))
      DELETE_THIS();
    return;
  } else if (result <= 0) {
    SendError(HTTP::BAD_REQUEST);
    return;
  } else {
    client_buffer_.erase(0, result);
  }

  ProcessRequestHeader();

  tunnel_ = request_.method().compare("CONNECT") == 0;

  if (tunnel_) {
    url_ = GURL("http://" + request_.path());
    if (!url_.has_port()) {
      SendError(HTTP::BAD_REQUEST);
      return;
    }
  } else {
    url_ = GURL(request_.path());
    if (!url_.is_valid()) {
      SendError(HTTP::BAD_REQUEST);
      return;
    }
    if (!config_->use_remote_proxy() && !url_.SchemeIs("http")) {
      SendError(HTTP::NOT_IMPLEMENTED);
      return;
    }

    if (request_.HeaderExists(kExpect)) {
      continue_ = ::_stricmp(request_.GetHeader(kExpect), "100-continue") == 0;
      if (!continue_) {
        SendError(HTTP::EXPECTATION_FAILED);
        return;
      }
    }
  }

  bool resolved = false;
  if (config_->use_remote_proxy())
    resolved = resolver_.Resolve(config_->remote_proxy_host().c_str(),
                                 config_->remote_proxy_port());
  else
    resolved = resolver_.Resolve(url_.host().c_str(), url_.EffectiveIntPort());

  if (!resolved) {
    SendError(HTTP::BAD_GATEWAY);
    return;
  }

  remote_.reset(new AsyncSocket());
  if (!remote_->ConnectAsync(*resolver_.begin(), this)) {
    SendError(HTTP::INTERNAL_SERVER_ERROR);
    return;
  }
}

void HttpProxySession::OnRequestSent(DWORD error, int length) {
  if (error != 0 || length == 0) {
    SendError(HTTP::SERVICE_UNAVAILABLE);
    return;
  }

  remote_buffer_.clear();
  phase_ = RequestBody;

  if (continue_) {
    phase_ = Response;
    if (remote_->ReceiveAsync(buffer_.get(), kBufferSize, 0, this))
      return;
  } else if (chunked_) {
    content_length_ = http::ParseChunk(client_buffer_, &chunk_size_);
    if (content_length_ == -2) {
      if (ReceiveAsync(client_, 0))
        return;
    } else if (content_length_ > 0) {
      if (remote_->SendAsync(client_buffer_.data(), content_length_, 0, this))
        return;
    }
  } else if (content_length_ > 0) {
    size_t length = min(client_buffer_.size(), content_length_);
    if (length > 0) {
      ::memmove(buffer_.get(), client_buffer_.data(), length);
      client_buffer_.erase(0, length);

      if (remote_->SendAsync(buffer_.get(), length, 0, this))
        return;
    } else {
      if (ReceiveAsync(client_, 0))
        return;
    }
  } else {
    phase_ = Response;
    if (remote_->ReceiveAsync(buffer_.get(), kBufferSize, 0, this))
      return;
  }

  SendError(HTTP::INTERNAL_SERVER_ERROR);
}

void HttpProxySession::OnRequestBodyReceived(DWORD error, int length) {
  if (error != 0 || length == 0) {
    DELETE_THIS();
    return;
  }

  if (chunked_) {
    client_buffer_.append(buffer_.get(), length);

    content_length_ = http::ParseChunk(client_buffer_, &chunk_size_);
    if (content_length_ == -2) {
      if (ReceiveAsync(client_, 0))
        return;
    } else if (content_length_ > 0) {
      if (remote_->SendAsync(client_buffer_.data(), content_length_, 0, this))
        return;
    }
  } else {
    if (remote_->SendAsync(buffer_.get(), length, 0, this))
      return;
  }

  SendError(HTTP::INTERNAL_SERVER_ERROR);
}

void HttpProxySession::OnRequestBodySent(DWORD error, int length) {
  if (error != 0 || length == 0) {
    SendError(HTTP::SERVICE_UNAVAILABLE);
    return;
  }

  if (chunked_) {
    // XXX(dacci): what if length < content_length_
    client_buffer_.erase(0, content_length_);

    if (chunk_size_ == 0) {
      phase_ = Response;
      if (remote_->ReceiveAsync(buffer_.get(), kBufferSize, 0, this))
        return;
    }

    content_length_ = http::ParseChunk(client_buffer_, &chunk_size_);
    if (content_length_ == -2) {
      if (ReceiveAsync(client_, 0))
        return;
    } else if (content_length_ > 0) {
      if (remote_->SendAsync(client_buffer_.data(), content_length_, 0, this))
        return;
    }
  } else {
    content_length_ -= length;
    if (content_length_ > 0) {
      if (ReceiveAsync(client_, 0))
        return;
    } else {
      phase_ = Response;
      if (remote_->ReceiveAsync(buffer_.get(), kBufferSize, 0, this))
        return;
    }
  }

  SendError(HTTP::INTERNAL_SERVER_ERROR);
}

void HttpProxySession::OnResponseReceived(DWORD error, int length) {
  if (error != 0 || length == 0) {
    SendError(HTTP::SERVICE_UNAVAILABLE);
    return;
  }

  remote_buffer_.append(buffer_.get(), length);
  if (remote_buffer_.size() > kBufferSize) {
    SendError(HTTP::BAD_GATEWAY);
    return;
  }

  int result = response_.Parse(remote_buffer_);
  if (result == HttpResponse::kPartial) {
    if (!remote_->ReceiveAsync(buffer_.get(), kBufferSize, 0, this))
      SendError(HTTP::INTERNAL_SERVER_ERROR);
    return;
  } else if (result <= 0) {
    SendError(HTTP::BAD_GATEWAY);
    return;
  } else {
    remote_buffer_.erase(0, result);
  }

  continue_ = response_.status() == HTTP::CONTINUE;
  if (!continue_)
    ProcessResponseHeader();

  std::string response_string;

  // XXX(dacci): isn't there any better way?
  if (tunnel_ && response_.status() == 200) {
    if (TunnelingService::Bind(client_, remote_)) {
      response_string += "HTTP/1.1 200 Connection Established\x0D\x0A";
      response_string += "Content-Length: 0\x0D\x0A";
      response_string += "\x0D\x0A";
      ::memmove(buffer_.get(), response_string.data(), response_string.size());

      if (!client_->SendAsync(buffer_.get(), response_string.size(), 0, this))
        DELETE_THIS();
    } else {
      SendError(HTTP::INTERNAL_SERVER_ERROR);
    }
    return;
  }

  response_string += "HTTP/1.";
  response_string += '0' + response_.minor_version();
  response_string += ' ';
  ::sprintf_s(buffer_.get(), kBufferSize, "%d ", response_.status());
  response_string += buffer_.get();
  response_string += response_.message();
  response_string += "\x0D\x0A";
  response_.SerializeHeaders(&response_string);
  response_string += "\x0D\x0A";
  if (response_string.size() > kBufferSize) {
    SendError(HTTP::BAD_GATEWAY);
    return;
  }

  ::memmove(buffer_.get(), response_string.data(), response_string.size());

  if (!client_->SendAsync(buffer_.get(), response_string.size(), 0, this)) {
    DELETE_THIS();
    return;
  }
}

void HttpProxySession::OnResponseSent(DWORD error, int length) {
  if (error != 0 || length == 0) {
    DELETE_THIS();
    return;
  }

  if (tunnel_) {
    if (response_.status() == 200) {
      client_ = NULL;
      remote_ = NULL;
    }

    DELETE_THIS();
    return;
  }

  if (request_.method().compare("HEAD") == 0) {
    EndSession();
    return;
  }

  phase_ = ResponseBody;

  if (continue_) {
    phase_ = RequestBody;

    if (client_buffer_.empty()) {
      if (ReceiveAsync(client_, 0))
        return;
    } else {
      assert(client_buffer_.size() <= kBufferSize);
      int length = client_buffer_.size();
      ::memmove(buffer_.get(), client_buffer_.data(), length);
      client_buffer_.clear();

      if (FireReceived(client_, 0, length))
        return;
    }
  } else if (chunked_) {
    content_length_ = http::ParseChunk(remote_buffer_, &chunk_size_);

    if (content_length_ == -2) {
      if (remote_->ReceiveAsync(buffer_.get(), kBufferSize, 0, this))
        return;
    } else if (content_length_ > 0) {
      if (client_->SendAsync(remote_buffer_.data(), content_length_, 0, this))
        return;
    }
  } else {
    if (content_length_ <= 0) {
      // no content, session end
      EndSession();
      return;
    }

    if (remote_buffer_.empty()) {
      if (remote_->ReceiveAsync(buffer_.get(), kBufferSize, 0, this))
        return;
    } else {
      if (client_->SendAsync(remote_buffer_.data(), remote_buffer_.size(), 0,
                             this))
        return;
    }
  }

  DELETE_THIS();
}

void HttpProxySession::OnResponseBodyReceived(DWORD error, int length) {
  if (error != 0 || length == 0) {
    DELETE_THIS();
    return;
  }

  if (chunked_) {
    remote_buffer_.append(buffer_.get(), length);

    content_length_ = http::ParseChunk(remote_buffer_, &chunk_size_);
    if (content_length_ == -2) {
      if (remote_->ReceiveAsync(buffer_.get(), kBufferSize, 0, this))
        return;
    } else if (content_length_ > 0) {
      if (client_->SendAsync(remote_buffer_.data(), content_length_, 0, this))
        return;
    }
  } else {
    if (client_->SendAsync(buffer_.get(), length, 0, this))
      return;
  }

  DELETE_THIS();
}

void HttpProxySession::OnResponseBodySent(DWORD error, int length) {
  if (error != 0 || length == 0) {
    DELETE_THIS();
    return;
  }

  if (chunked_) {
    // XXX(dacci): what if length < content_length_
    remote_buffer_.erase(0, content_length_);

    if (chunk_size_ == 0) {
      // final chunk have sent, session end
      EndSession();
      return;
    }

    content_length_ = http::ParseChunk(remote_buffer_, &chunk_size_);
    if (content_length_ == -2) {
      if (remote_->ReceiveAsync(buffer_.get(), kBufferSize, 0, this))
        return;
    } else if (content_length_ > 0) {
      if (client_->SendAsync(remote_buffer_.data(), content_length_, 0, this))
        return;
    }
  } else {
    if (content_length_ > 0) {
      if (content_length_ <= length) {
        // no more content, session end
        EndSession();
        return;
      }

      content_length_ -= length;
    }

    if (remote_->ReceiveAsync(buffer_.get(), kBufferSize, 0, this))
      return;
  }

  DELETE_THIS();
}

void CALLBACK HttpProxySession::OnTimeout(PTP_CALLBACK_INSTANCE instance,
                                          PVOID param, PTP_TIMER timer) {
  HttpProxySession* session = static_cast<HttpProxySession*>(param);
  session->receiving_->Shutdown(SD_BOTH);
}

void CALLBACK HttpProxySession::DeleteThis(PTP_CALLBACK_INSTANCE instance,
                                           void* param) {
  delete static_cast<HttpProxySession*>(param);
}
