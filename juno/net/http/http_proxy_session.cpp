// Copyright (c) 2013 dacci.org

#include "net/http/http_proxy_session.h"

#include <assert.h>

#include "misc/string_util.h"
#include "net/tunneling_service.h"

const std::string HttpProxySession::kConnection("Connection");
const std::string HttpProxySession::kContentEncoding("Content-Encoding");
const std::string HttpProxySession::kContentLength("Content-Length");
const std::string HttpProxySession::kExpect("Expect");
const std::string HttpProxySession::kKeepAlive("Keep-Alive");
const std::string HttpProxySession::kProxyAuthenticate("Proxy-Authenticate");
const std::string HttpProxySession::kProxyAuthorization("Proxy-Authorization");
const std::string HttpProxySession::kProxyConnection("Proxy-Connection");
const std::string HttpProxySession::kTransferEncoding("Transfer-Encoding");

HttpProxySession::HttpProxySession(AsyncSocket* client)
    : client_(client),
      remote_(),
      buffer_(new char[kBufferSize]),
      phase_(Request),
      close_client_(),
      timer_() {
}

HttpProxySession::~HttpProxySession() {
  if (timer_ != NULL) {
    ::DeleteTimerQueueTimer(NULL, timer_, INVALID_HANDLE_VALUE);
    timer_ = NULL;
  }

  if (client_ != NULL) {
    client_->Shutdown(SD_BOTH);
    delete client_;
    client_ = NULL;
  }

  if (remote_ != NULL) {
    remote_->Shutdown(SD_BOTH);
    delete remote_;
    remote_ = NULL;
  }

  if (buffer_ != NULL) {
    delete[] buffer_;
    buffer_ = NULL;
  }
}

bool HttpProxySession::Start() {
  return ReceiveAsync(client_, 0);
}

void HttpProxySession::OnConnected(AsyncSocket* socket, DWORD error) {
  if (error != 0) {
    SendError(HTTP::BAD_GATEWAY);
    return;
  }

  std::string request_string;

  if (tunnel_) {
    if (!TunnelingService::Bind(client_, remote_)) {
      SendError(HTTP::INTERNAL_SERVER_ERROR);
      return;
    }

    request_string += "HTTP/1.1 200 Connection Established\x0D\x0A";
    request_string += "Content-Length: 0\x0D\x0A";
    request_string += "\x0D\x0A";
    ::memmove(buffer_, request_string.data(), request_string.size());

    if (!client_->SendAsync(buffer_, request_string.size(), 0, this)) {
      delete this;
      return;
    }
  } else {
    request_string += request_.method();
    request_string += ' ';
    request_string += url_.GetUrlPath();
    request_string += url_.GetExtraInfo();
    request_string += " HTTP/1.";
    request_string += '0' + request_.minor_version();
    request_string += "\x0D\x0A";
    request_.SerializeHeaders(&request_string);
    request_string += "\x0D\x0A";
    if (request_string.size() > kBufferSize) {
      SendError(HTTP::REQUEST_ENTITY_TOO_LARGE);
      return;
    }

    ::memmove(buffer_, request_string.data(), request_string.size());

    if (!remote_->SendAsync(buffer_, request_string.size(), 0, this)) {
      SendError(HTTP::INTERNAL_SERVER_ERROR);
      return;
    }
  }
}

void HttpProxySession::OnReceived(AsyncSocket* socket, DWORD error,
                                  int length) {
  assert(timer_ != NULL);
  ::DeleteTimerQueueTimer(NULL, timer_, INVALID_HANDLE_VALUE);
  timer_ = NULL;

  switch (phase_) {
  case Request:
    OnRequestReceived(socket, error, length);
    return;

  case RequestBody:
    OnRequestBodyReceived(socket, error, length);
    return;

  case Response:
    OnResponseReceived(socket, error, length);
    return;

  case ResponseBody:
    OnResponseBodyReceived(socket, error, length);
    return;
  }

  assert(false);
}

void HttpProxySession::OnSent(AsyncSocket* socket, DWORD error, int length) {
  switch (phase_) {
  case Request:
    OnRequestSent(socket, error, length);
    return;

  case RequestBody:
    OnRequestBodySent(socket, error, length);
    return;

  case Response:
    OnResponseSent(socket, error, length);
    return;

  case ResponseBody:
    OnResponseBodySent(socket, error, length);
    return;

  case Error:
    delete this;
    return;
  }

  assert(false);
}

bool HttpProxySession::ReceiveAsync(AsyncSocket* socket, int flags) {
  assert(timer_ == NULL);
  if (!::CreateTimerQueueTimer(&timer_, NULL, OnTimeout, this, kTimeout, 0,
                               WT_EXECUTEDEFAULT))
    return false;

  receiving_ = socket;

  return socket->ReceiveAsync(buffer_, kBufferSize, flags, this);
}

int64_t HttpProxySession::ParseChunk(const std::string& buffer,
                                     int64_t* chunk_size) {
  char* start = const_cast<char*>(buffer.c_str());
  char* end = start;
  *chunk_size = ::_strtoi64(buffer.c_str(), &end, 16);

  while (*end) {
    if (*end != '\x0D' && *end != '\x0A')
      return -1;

    if (*end == '\x0A')
      break;

    ++end;
  }

  if (*end == '\0')
    return -2;

  ++end;

  int64_t length = *chunk_size + (end - start);
  if (buffer.size() < length)
    return -2;

  end = start + length;

  while (*end) {
    if (*end != '\x0D' && *end != '\x0A')
      return -1;

    if (*end == '\x0A')
      break;

    ++end;
  }

  if (*end == '\0')
    return -2;

  ++end;

  return end - start;
}

void HttpProxySession::ProcessRequestHeader() {
  content_length_ = 0;
  chunked_ = false;
  ProcessMessageLength(&request_);
  if (request_.minor_version() < 1)
    close_client_ = true;

  ProcessHopByHopHeaders(&request_);

  // TODO(dacci): process user defined header modification
}

void HttpProxySession::ProcessResponseHeader() {
  content_length_ = -2;
  chunked_ = false;
  ProcessMessageLength(&response_);
  if (!chunked_ && content_length_ < 0)
    close_client_ = true;

  ProcessHopByHopHeaders(&response_);

  if (close_client_)
    response_.AddHeader(kConnection, "close");

  // TODO(dacci): process user defined header modification
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
  headers->RemoveHeader(kProxyAuthenticate);
  headers->RemoveHeader(kProxyAuthorization);
  headers->RemoveHeader(kProxyConnection);
}

void HttpProxySession::SendError(HTTP::StatusCode status) {
  phase_ = Error;
  close_client_ = true;

  const char* message = HTTP::GetStatusMessage(status);
  if (message == NULL)
    message = HTTP::GetStatusMessage(HTTP::INTERNAL_SERVER_ERROR);

  int length = ::sprintf_s(buffer_, kBufferSize,
                           "HTTP/1.1 %d %s\x0D\x0A"
                           "Content-Length: 0\x0D\x0A"
                           "Connection: close\x0D\x0A"
                           "\x0D\x0A", status, message);
  if (length <= 0 || !client_->SendAsync(buffer_, length, 0, this))
    delete this;
}

void HttpProxySession::EndSession() {
  if (close_client_) {
    delete this;
    return;
  }

  if (remote_ != NULL) {
    remote_->Shutdown(SD_BOTH);
    delete remote_;
    remote_ = NULL;
  }

  request_.Clear();
  url_.Clear();
  resolver_.Free();

  remote_buffer_.clear();
  response_.Clear();

  phase_ = Request;
  close_client_ = false;

  if (client_buffer_.empty()) {
    Start();
  } else {
    int length = client_buffer_.size();
    ::memmove(buffer_, client_buffer_.data(), length);
    client_buffer_.clear();

    FireReceived(client_, 0, length);
  }
}

bool HttpProxySession::FireReceived(AsyncSocket* socket, DWORD error,
                                    int length) {
  EventArgs* args = new EventArgs;
  if (args == NULL)
    return false;

  ::memset(args, 0, sizeof(*args));

  args->event = Received;
  args->session = this;
  args->socket = socket;
  args->error = error;
  args->length = length;

  if (!::QueueUserWorkItem(FireEvent, args, 0)) {
    delete args;
    return false;
  }

  return true;
}

DWORD CALLBACK HttpProxySession::FireEvent(void* param) {
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

  return 0;
}

void HttpProxySession::OnRequestReceived(AsyncSocket* socket, DWORD error,
                                         int length) {
  if (error != 0 || length == 0) {
    delete this;
    return;
  }

  client_buffer_.append(buffer_, length);
  if (client_buffer_.size() > kBufferSize) {
    SendError(HTTP::REQUEST_ENTITY_TOO_LARGE);
    return;
  }

  int result = request_.Parse(client_buffer_);
  if (result == HttpRequest::kPartial) {
    if (!ReceiveAsync(client_, 0))
      delete this;
    return;
  } else if (result <= 0) {
    SendError(HTTP::BAD_REQUEST);
    return;
  } else {
    client_buffer_.erase(0, result);
  }

  ProcessRequestHeader();

  tunnel_ = request_.method().compare("CONNECT") == 0;

  if (!url_.CrackUrl(request_.path().c_str())) {
    SendError(HTTP::BAD_REQUEST);
    return;
  }
  if (!tunnel_ && url_.GetScheme() != ATL_URL_SCHEME_HTTP) {
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

  bool resolved = false;
  if (tunnel_) {
    resolved = resolver_.Resolve(url_.GetSchemeName(), url_.GetHostName());
  } else {
    char service[8];
    ::sprintf_s(service, "%d", url_.GetPortNumber());
    resolved = resolver_.Resolve(url_.GetHostName(), service);
  }
  if (!resolved) {
    SendError(HTTP::BAD_GATEWAY);
    return;
  }

  remote_ = new AsyncSocket();
  if (!remote_->ConnectAsync(*resolver_, this)) {
    SendError(HTTP::INTERNAL_SERVER_ERROR);
    return;
  }
}

void HttpProxySession::OnRequestSent(AsyncSocket* socket, DWORD error,
                                     int length) {
  if (error != 0 || length == 0) {
    SendError(HTTP::SERVICE_UNAVAILABLE);
    return;
  }

  phase_ = RequestBody;

  if (tunnel_) {
    client_ = NULL;
    remote_ = NULL;
    delete this;
    return;
  } else if (continue_) {
    phase_ = Response;
    if (ReceiveAsync(remote_, 0))
      return;
  } else if (chunked_) {
    content_length_ = ParseChunk(client_buffer_, &chunk_size_);
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
      ::memmove(buffer_, client_buffer_.data(), length);
      client_buffer_.erase(0, length);

      if (remote_->SendAsync(buffer_, length, 0, this))
        return;
    } else {
      if (ReceiveAsync(client_, 0))
        return;
    }
  } else {
    phase_ = Response;
    if (ReceiveAsync(remote_, 0))
      return;
  }

  SendError(HTTP::INTERNAL_SERVER_ERROR);
}

void HttpProxySession::OnRequestBodyReceived(AsyncSocket* socket, DWORD error,
                                             int length) {
  if (error != 0 || length == 0) {
    delete this;
    return;
  }

  if (chunked_) {
    client_buffer_.append(buffer_, length);

    content_length_ = ParseChunk(client_buffer_, &chunk_size_);
    if (content_length_ == -2) {
      if (ReceiveAsync(client_, 0))
        return;
    } else if (content_length_ > 0) {
      if (remote_->SendAsync(client_buffer_.data(), content_length_, 0, this))
        return;
    }
  } else {
    if (remote_->SendAsync(buffer_, length, 0, this))
      return;
  }

  SendError(HTTP::INTERNAL_SERVER_ERROR);
}

void HttpProxySession::OnRequestBodySent(AsyncSocket* socket, DWORD error,
                                         int length) {
  if (error != 0 || length == 0) {
    SendError(HTTP::SERVICE_UNAVAILABLE);
    return;
  }

  if (chunked_) {
    // XXX(dacci): what if length < content_length_
    client_buffer_.erase(0, content_length_);

    if (chunk_size_ == 0) {
      phase_ = Response;
      if (ReceiveAsync(remote_, 0))
        return;
    }

    content_length_ = ParseChunk(client_buffer_, &chunk_size_);
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
      if (ReceiveAsync(remote_, 0))
        return;
    }
  }

  SendError(HTTP::INTERNAL_SERVER_ERROR);
}

void HttpProxySession::OnResponseReceived(AsyncSocket* socket, DWORD error,
                                          int length) {
  if (error != 0 || length == 0) {
    SendError(HTTP::SERVICE_UNAVAILABLE);
    return;
  }

  remote_buffer_.append(buffer_, length);
  if (remote_buffer_.size() > kBufferSize) {
    SendError(HTTP::BAD_GATEWAY);
    return;
  }

  int result = response_.Parse(remote_buffer_);
  if (result == HttpResponse::kPartial) {
    if (!ReceiveAsync(remote_, 0))
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
  response_string += "HTTP/1.";
  response_string += '0' + response_.minor_version();
  response_string += ' ';
  ::sprintf_s(buffer_, kBufferSize, "%d ", response_.status());
  response_string += buffer_;
  response_string += response_.message();
  response_string += "\x0D\x0A";
  response_.SerializeHeaders(&response_string);
  response_string += "\x0D\x0A";
  if (response_string.size() > kBufferSize) {
    SendError(HTTP::BAD_GATEWAY);
    return;
  }

  ::memmove(buffer_, response_string.data(), response_string.size());

  if (!client_->SendAsync(buffer_, response_string.size(), 0, this)) {
    delete this;
    return;
  }
}

void HttpProxySession::OnResponseSent(AsyncSocket* socket, DWORD error,
                                      int length) {
  if (error != 0 || length == 0) {
    delete this;
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
      ::memmove(buffer_, client_buffer_.data(), length);
      client_buffer_.clear();

      if (FireReceived(client_, 0, length))
        return;
    }
  } else if (chunked_) {
    content_length_ = ParseChunk(remote_buffer_, &chunk_size_);

    if (content_length_ == -2) {
      if (ReceiveAsync(remote_, 0))
        return;
    } else if (content_length_ > 0) {
      if (client_->SendAsync(remote_buffer_.data(), content_length_, 0, this))
        return;
    }
  } else {
    if (content_length_ == 0) {
      // no content, session end
      EndSession();
      return;
    }

    if (remote_buffer_.empty()) {
      if (ReceiveAsync(remote_, 0))
        return;
    } else {
      if (client_->SendAsync(remote_buffer_.data(), remote_buffer_.size(), 0,
                             this))
        return;
    }
  }

  delete this;
}

void HttpProxySession::OnResponseBodyReceived(AsyncSocket* socket, DWORD error,
                                              int length) {
  if (error != 0 || length == 0) {
    delete this;
    return;
  }

  if (chunked_) {
    remote_buffer_.append(buffer_, length);

    content_length_ = ParseChunk(remote_buffer_, &chunk_size_);
    if (content_length_ == -2) {
      if (ReceiveAsync(remote_, 0))
        return;
    } else if (content_length_ > 0) {
      if (client_->SendAsync(remote_buffer_.data(), content_length_, 0, this))
        return;
    }
  } else {
    if (client_->SendAsync(buffer_, length, 0, this))
      return;
  }

  delete this;
}

void HttpProxySession::OnResponseBodySent(AsyncSocket* socket, DWORD error,
                                          int length) {
  if (error != 0 || length == 0) {
    delete this;
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

    content_length_ = ParseChunk(remote_buffer_, &chunk_size_);
    if (content_length_ == -2) {
      if (ReceiveAsync(remote_, 0))
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

    if (ReceiveAsync(remote_, 0))
      return;
  }

  delete this;
}

void CALLBACK HttpProxySession::OnTimeout(void* param, BOOLEAN fired) {
  HttpProxySession* session = static_cast<HttpProxySession*>(param);
  session->receiving_->Shutdown(SD_BOTH);
}
