// Copyright (c) 2013 dacci.org

#include "net/http/http_proxy_session.h"

#include <assert.h>

#include "misc/string_util.h"
#include "net/tunneling_service.h"

HttpProxySession::HttpProxySession(AsyncSocket* client)
    : client_(client),
      remote_(),
      buffer_(new char[kBufferSize]),
      phase_(Request) {
}

HttpProxySession::~HttpProxySession() {
  if (client_ != NULL) {
    client_->Shutdown(SD_BOTH);
    delete client_;
  }

  if (remote_ != NULL) {
    remote_->Shutdown(SD_BOTH);
    delete remote_;
  }

  delete[] buffer_;
}

bool HttpProxySession::Start() {
  return client_->ReceiveAsync(buffer_, kBufferSize, 0, this);
}

void HttpProxySession::OnConnected(AsyncSocket* socket, DWORD error) {
  if (error != 0) {
    delete this;
    return;
  }

  std::string request_string;

  if (tunnel_) {
    if (!TunnelingService::Bind(client_, remote_)) {
      delete this;
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
      delete this;
      return;
    }

    ::memmove(buffer_, request_string.data(), request_string.size());

    if (!remote_->SendAsync(buffer_, request_string.size(), 0, this)) {
      delete this;
      return;
    }
  }
}

void HttpProxySession::OnReceived(AsyncSocket* socket, DWORD error,
                                  int length) {
  if (error != 0 || length == 0) {
    delete this;
    return;
  }

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
  if (error != 0 || length == 0) {
    delete this;
    return;
  }

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
  }

  assert(false);
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

void HttpProxySession::OnRequestReceived(AsyncSocket* socket, DWORD error,
                                         int length) {
  client_buffer_.append(buffer_, length);
  int result = request_.Parse(client_buffer_);
  if (result == HttpRequest::kPartial) {
    if (!client_->ReceiveAsync(buffer_, kBufferSize, 0, this))
      delete this;
    return;
  } else if (result <= 0) {
    delete this;
    return;
  } else {
    client_buffer_.erase(0, result);
  }

  tunnel_ = request_.method().compare("CONNECT") == 0;

  // RFC 2616 - 4.4 Message Length
  content_length_ = 0;
  chunked_ = false;
  if (request_.HeaderExists("Transfer-Encoding") &&
      ::_stricmp(request_.GetHeader("Transfer-Encoding"), "identity") != 0) {
    chunked_ = true;
  } else if (request_.HeaderExists("Content-Length")) {
    const std::string& value = request_.GetHeader("Content-Length");
    content_length_ = ::_strtoi64(value.c_str(), NULL, 10);
  }

  if (!url_.CrackUrl(request_.path().c_str())) {
    delete this;
    return;
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
    delete this;
    return;
  }

  remote_ = new AsyncSocket();
  if (!remote_->ConnectAsync(*resolver_, this)) {
    delete this;
    return;
  }
}

void HttpProxySession::OnRequestSent(AsyncSocket* socket, DWORD error,
                                     int length) {
  phase_ = RequestBody;

  if (tunnel_) {
    client_ = NULL;
    remote_ = NULL;
    delete this;
    return;
  } else if (chunked_) {
    content_length_ = ParseChunk(client_buffer_, &chunk_size_);
    if (content_length_ == -2) {
      if (!client_->ReceiveAsync(buffer_, kBufferSize, 0, this))
        delete this;
      return;
    } else if (content_length_ <= 0) {
      delete this;
      return;
    } else {
      if (!remote_->SendAsync(client_buffer_.data(), content_length_, 0, this))
        delete this;
      return;
    }
  } else if (content_length_ > 0) {
    size_t length = min(client_buffer_.size(), content_length_);
    if (length > 0) {
      ::memmove(buffer_, client_buffer_.data(), length);
      client_buffer_.erase(0, length);

      if (!remote_->SendAsync(buffer_, length, 0, this)) {
        delete this;
        return;
      }
    } else {
      if (!client_->ReceiveAsync(buffer_, kBufferSize, 0, this)) {
        delete this;
        return;
      }
    }
  } else {
    phase_ = Response;
    if (!remote_->ReceiveAsync(buffer_, kBufferSize, 0, this)) {
      delete this;
      return;
    }
  }
}

void HttpProxySession::OnRequestBodyReceived(AsyncSocket* socket, DWORD error,
                                             int length) {
  if (chunked_) {
    client_buffer_.append(buffer_, length);

    content_length_ = ParseChunk(client_buffer_, &chunk_size_);
    if (content_length_ == -2) {
      if (!client_->ReceiveAsync(buffer_, kBufferSize, 0, this))
        delete this;
      return;
    } else if (content_length_ <= 0) {
      delete this;
      return;
    } else {
      if (!remote_->SendAsync(client_buffer_.data(), content_length_, 0, this))
        delete this;
      return;
    }
  } else {
    if (!remote_->SendAsync(buffer_, length, 0, this))
      delete this;
    return;
  }
}

void HttpProxySession::OnRequestBodySent(AsyncSocket* socket, DWORD error,
                                         int length) {
  if (chunked_) {
    // XXX(dacci): what if length < content_length_
    client_buffer_.erase(0, content_length_);

    if (chunk_size_ == 0) {
      phase_ = Response;
      if (!remote_->ReceiveAsync(buffer_, kBufferSize, 0, this))
        delete this;
      return;
    }

    content_length_ = ParseChunk(client_buffer_, &chunk_size_);
    if (content_length_ == -2) {
      if (!client_->ReceiveAsync(buffer_, kBufferSize, 0, this))
        delete this;
      return;
    } else if (content_length_ <= 0) {
      delete this;
      return;
    } else {
      if (!remote_->SendAsync(client_buffer_.data(), content_length_, 0, this))
        delete this;
      return;
    }
  } else {
    content_length_ -= length;
    if (content_length_ > 0) {
      if (!client_->ReceiveAsync(buffer_, kBufferSize, 0, this)) {
        delete this;
        return;
      }
    } else {
      phase_ = Response;
      if (!remote_->ReceiveAsync(buffer_, kBufferSize, 0, this)) {
        delete this;
        return;
      }
    }
  }
}

void HttpProxySession::OnResponseReceived(AsyncSocket* socket, DWORD error,
                                          int length) {
  remote_buffer_.append(buffer_, length);
  int result = response_.Parse(remote_buffer_);
  if (result == HttpResponse::kPartial) {
    if (!remote_->ReceiveAsync(buffer_, kBufferSize, 0, this))
      delete this;
    return;
  } else if (result <= 0) {
    delete this;
    return;
  } else {
    remote_buffer_.erase(0, result);
  }

  // RFC 2616 - 4.4 Message Length
  content_length_ = -2;
  chunked_ = false;
  if (response_.HeaderExists("Transfer-Encoding") &&
      ::_stricmp(response_.GetHeader("Transfer-Encoding"), "identity") != 0) {
    chunked_ = true;
  } else if (response_.HeaderExists("Content-Length")) {
    const std::string& value = response_.GetHeader("Content-Length");
    content_length_ = ::_strtoi64(value.c_str(), NULL, 10);
  }

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
    delete this;
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
  phase_ = ResponseBody;

  if (chunked_) {
    content_length_ = ParseChunk(remote_buffer_, &chunk_size_);

    if (content_length_ == -2) {
      if (!remote_->ReceiveAsync(buffer_, kBufferSize, 0, this))
        delete this;
      return;
    } else if (content_length_ < 0) {
      delete this;
      return;
    }

    if (!client_->SendAsync(remote_buffer_.data(), content_length_, 0, this))
      delete this;
    return;
  } else {
    if (content_length_ == 0) {
      // no content, session end
      delete this;
      return;
    }

    if (remote_buffer_.empty()) {
      if (!remote_->ReceiveAsync(buffer_, kBufferSize, 0, this)) {
        delete this;
        return;
      }
    } else {
      if (!client_->SendAsync(remote_buffer_.data(), remote_buffer_.size(), 0,
                              this)) {
        delete this;
        return;
      }
    }
  }
}

void HttpProxySession::OnResponseBodyReceived(AsyncSocket* socket, DWORD error,
                                              int length) {
  if (chunked_) {
    if (content_length_ == -2) {
      remote_buffer_.append(buffer_, length);

      content_length_ = ParseChunk(remote_buffer_, &chunk_size_);
      if (content_length_ == -2) {
        if (!remote_->ReceiveAsync(buffer_, kBufferSize, 0, this)) {
          delete this;
          return;
        }
      } else {
        if (!client_->SendAsync(remote_buffer_.data(), content_length_, 0,
                                this)) {
          delete this;
          return;
        }
      }
    } else if (content_length_ <= 0) {
      delete this;
      return;
    } else {
      if (!client_->SendAsync(remote_buffer_.data(), content_length_, 0,
                              this)) {
        delete this;
        return;
      }
    }
  } else {
    if (!client_->SendAsync(buffer_, length, 0, this)) {
      delete this;
      return;
    }
  }
}

void HttpProxySession::OnResponseBodySent(AsyncSocket* socket, DWORD error,
                                          int length) {
  if (chunked_) {
    // XXX(dacci): what if length < content_length_
    remote_buffer_.erase(0, content_length_);

    if (chunk_size_ == 0) {
      delete this;
      return;
    }

    content_length_ = ParseChunk(remote_buffer_, &chunk_size_);
    if (content_length_ == -2) {
      if (!remote_->ReceiveAsync(buffer_, kBufferSize, 0, this)) {
        delete this;
        return;
      }
    } else if (content_length_ <= 0) {
      delete this;
      return;
    } else {
      if (!client_->SendAsync(remote_buffer_.data(), content_length_, 0,
                              this)) {
        delete this;
        return;
      }
    }
  } else {
    if (content_length_ > 0) {
      if (content_length_ <= length) {
        // no more content, session end
        delete this;
        return;
      }

      content_length_ -= length;
    }

    if (!remote_->ReceiveAsync(buffer_, kBufferSize, 0, this)) {
      delete this;
      return;
    }
  }
}
