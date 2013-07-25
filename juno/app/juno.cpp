// Copyright (c) 2013 dacci.org

#include <crtdbg.h>
#include <stdint.h>

#include <atlbase.h>
#include <atlutil.h>

#include <madoka/net/winsock.h>

#include <string>
#include <vector>

#include "net/async_socket.h"
#include "net/async_server_socket.h"
#include "net/http/http_request.h"
#include "net/http/http_response.h"

class HttpProxySession : public AsyncSocket::Listener {
 public:
  explicit HttpProxySession(AsyncSocket* client)
      : client_(client),
        remote_(),
        buffer_(new char[kBufferSize]),
        phase_(Request),
        content_length_() {
  }

  virtual ~HttpProxySession() {
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

  bool Start() {
    return client_->ReceiveAsync(buffer_, kBufferSize, 0, this);
  }

  void OnConnected(AsyncSocket* socket, DWORD error) {
    if (error != 0) {
      delete this;
      return;
    }

    std::string request_string;
    request_string += request_.method();
    request_string += ' ';
    request_string += url_.GetUrlPath();
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

  void OnReceived(AsyncSocket* socket, DWORD error, int length) {
    if (error != 0) {
      delete this;
      return;
    }

    if (phase_ == Request) {
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

      if (request_.HeaderExists("Content-Length")) {
        const std::string& value = request_.GetHeader("Content-Length");
        content_length_ = ::_strtoi64(value.c_str(), NULL, 10);
      } else if (request_.HeaderExists("Transfer-Encoding")) {
        delete this;
        return;
      }

      if (!url_.CrackUrl(request_.path().c_str())) {
        delete this;
        return;
      }

      char service[8];
      ::sprintf_s(service, "%d", url_.GetPortNumber());

      if (!resolver_.Resolve(url_.GetHostName(), service)) {
        delete this;
        return;
      }

      remote_ = new AsyncSocket();
      if (!remote_->ConnectAsync(*resolver_, this)) {
        delete this;
        return;
      }
    } else if (phase_ == RequestBody) {
      if (!remote_->SendAsync(buffer_, length, 0, this))
        delete this;
      return;
    } else if (phase_ == Response) {
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

      if (response_.HeaderExists("Content-Length")) {
        const std::string& value = response_.GetHeader("Content-Length");
        content_length_ = ::_strtoi64(value.c_str(), NULL, 10);
      } else if (response_.HeaderExists("Transfer-Encoding")) {
        delete this;
        return;
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
    } else if (phase_ == ResponseBody) {
      if (!client_->SendAsync(buffer_, length, 0, this)) {
        delete this;
        return;
      }
    }
  }

  void OnSent(AsyncSocket* socket, DWORD error, int length) {
    if (error != 0) {
      delete this;
      return;
    }

    if (phase_ == Request) {
      if (content_length_ > 0) {
        phase_ = RequestBody;

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
    } else if (phase_ == RequestBody) {
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
    } else if (phase_ == Response) {
      phase_ = ResponseBody;

      if (content_length_ > 0) {
        if (remote_buffer_.empty()) {
          if (!remote_->ReceiveAsync(buffer_, kBufferSize, 0, this)) {
            delete this;
            return;
          }
        } else {
          content_length_ -= remote_buffer_.size();
          if (!client_->SendAsync(remote_buffer_.data(), remote_buffer_.size(),
                                  0, this)) {
            delete this;
            return;
          }
        }
      } else {
        delete this;
        return;
      }
    } else if (phase_ == ResponseBody) {
      if (!remote_->ReceiveAsync(buffer_, kBufferSize, 0, this)) {
        delete this;
        return;
      }
    }
  }

 private:
  enum Phase {
    Request, RequestBody, Response, ResponseBody,
  };

  static const size_t kBufferSize = 4096;

  AsyncSocket* client_;
  std::string client_buffer_;
  HttpRequest request_;
  CUrl url_;
  madoka::net::AddressInfo resolver_;

  AsyncSocket* remote_;
  std::string remote_buffer_;
  HttpResponse response_;

  char* buffer_;
  Phase phase_;
  int64_t content_length_;
};

class HttpProxy : public AsyncServerSocket::Listener {
 public:
  HttpProxy(const char* address, const char* port)
      : address_(address), port_(port) {
  }

  bool Start() {
    resolver_.ai_flags = AI_PASSIVE;
    resolver_.ai_socktype = SOCK_STREAM;
    if (!resolver_.Resolve(address_.c_str(), port_.c_str()))
      return false;

    bool started = false;

    for (auto i = resolver_.begin(), l = resolver_.end(); i != l; ++i) {
      AsyncServerSocket* server = new AsyncServerSocket();
      if (server->Bind(*i) && server->Listen(SOMAXCONN) &&
          server->AcceptAsync(this)) {
        started = true;
        servers_.push_back(server);
      }
    }

    return started;
  }

  void Stop() {
    for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i) {
      (*i)->Close();
      delete *i;
    }
    servers_.clear();
  }

  void OnAccepted(AsyncServerSocket* server, AsyncSocket* client, DWORD error) {
    if (error == 0) {
      server->AcceptAsync(this);

      HttpProxySession* session = new HttpProxySession(client);
      if (!session->Start())
        delete session;
    } else {
      delete client;
    }
  }

 private:
  std::string address_;
  std::string port_;
  madoka::net::AddressInfo resolver_;
  std::vector<AsyncServerSocket*> servers_;
};

int main(int argc, char* argv[]) {
#ifdef _DEBUG
  {
    int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    flags |= _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF;
    _CrtSetDbgFlag(flags);
  }
#endif  // _DEBUG

  const char* port = "8080";
  if (argc >= 2)
    port = argv[1];

  const char* address = "127.0.0.1";
  if (argc >= 3)
    address = argv[2];

  madoka::net::WinSock winsock(WINSOCK_VERSION);
  if (!winsock.Initialized())
    return winsock.error();

  HttpProxy proxy(address, port);
  if (!proxy.Start())
    return __LINE__;

  ::getchar();

  proxy.Stop();

  return 0;
}
