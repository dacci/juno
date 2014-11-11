// Copyright (c) 2013 dacci.org

#include "service/socks/socks_proxy_session.h"

#include <assert.h>

#include <madoka/net/resolver.h>

#include <string>

#include "net/tunneling_service.h"
#include "service/socks/socks_proxy.h"

using ::madoka::net::AsyncSocket;

namespace {

struct SocketAddress4 : addrinfo, sockaddr_in {
  SocketAddress4() : addrinfo(), sockaddr_in() {
    ai_family = AF_INET;
    ai_addrlen = sizeof(sockaddr_in);
    ai_addr = reinterpret_cast<sockaddr*>(static_cast<sockaddr_in*>(this));

    sin_family = ai_family;
  }

  operator sockaddr*() {
    return ai_addr;
  }
};

struct SocketAddress6 : addrinfo, sockaddr_in6 {
  SocketAddress6() : addrinfo(), sockaddr_in6() {
    ai_family = AF_INET6;
    ai_addrlen = sizeof(sockaddr_in6);
    ai_addr = reinterpret_cast<sockaddr*>(static_cast<sockaddr_in6*>(this));

    sin6_family = ai_family;
  }

  operator sockaddr*() {
    return ai_addr;
  }
};

}  // namespace

#define DELETE_THIS() \
  ::TrySubmitThreadpoolCallback(DeleteThis, this, nullptr)

SocksProxySession::SocksProxySession(SocksProxy* proxy)
    : proxy_(proxy),
      client_(),
      remote_(),
      request_buffer_(new char[kBufferSize]),
      response_buffer_(new char[kBufferSize]),
      phase_(),
      end_point_() {
}

SocksProxySession::~SocksProxySession() {
  Stop();

  proxy_->EndSession(this);
}

bool SocksProxySession::Start(const Service::AsyncSocketPtr& client) {
  client_ = client;
  client_->ReceiveAsync(request_buffer_.get(), kBufferSize, 0, this);

  return true;
}

void SocksProxySession::Stop() {
  if (client_ != nullptr)
    client_->Shutdown(SD_BOTH);

  if (remote_ != nullptr) {
    if (remote_->connected())
      remote_->Shutdown(SD_BOTH);
    else
      remote_->CancelAsyncConnect();
  }
}

void SocksProxySession::OnConnected(AsyncSocket* socket, DWORD error) {
  if (request_buffer_[0] == 4) {
    SOCKS4::REQUEST* request =
        reinterpret_cast<SOCKS4::REQUEST*>(request_buffer_.get());

    if (request->address.s_addr != 0 &&
        ::htonl(request->address.s_addr) <= 0x000000FF)
      delete static_cast<madoka::net::Resolver*>(end_point_);
    else
      delete static_cast<SocketAddress4*>(end_point_);

    SOCKS4::RESPONSE* response =
        reinterpret_cast<SOCKS4::RESPONSE*>(response_buffer_.get());

    if (error == 0 && TunnelingService::Bind(client_, remote_))
        response->code = SOCKS4::GRANTED;

    client_->SendAsync(response, sizeof(*response), 0, this);
  } else if (request_buffer_[0] == 5) {
    SOCKS5::REQUEST* request =
        reinterpret_cast<SOCKS5::REQUEST*>(request_buffer_.get());

    SOCKS5::RESPONSE* response =
        reinterpret_cast<SOCKS5::RESPONSE*>(response_buffer_.get());
    int response_length = 10;

    switch (request->type) {
      case SOCKS5::IP_V4:
        delete static_cast<SocketAddress4*>(end_point_);
        break;

      case SOCKS5::DOMAINNAME:
        delete static_cast<madoka::net::Resolver*>(end_point_);
        break;

      case SOCKS5::IP_V6:
        delete static_cast<SocketAddress6*>(end_point_);
        break;
    }

    if (error == 0) {
      sockaddr_storage address;
      int length = sizeof(address);

      if (remote_->GetLocalEndPoint(reinterpret_cast<sockaddr*>(&address),
                                    &length)) {
        switch (address.ss_family) {
          case AF_INET: {
            sockaddr_in* address4 = reinterpret_cast<sockaddr_in*>(&address);
            response->type = SOCKS5::IP_V4;
            response->address.ipv4.ipv4_addr = address4->sin_addr;
            response->address.ipv4.ipv4_port = address4->sin_port;
            break;
          }

          case AF_INET6: {
            sockaddr_in6* address6 = reinterpret_cast<sockaddr_in6*>(&address);
            response->type = SOCKS5::IP_V6;
            response->address.ipv6.ipv6_addr = address6->sin6_addr;
            response->address.ipv6.ipv6_port = address6->sin6_port;
            response_length += 12;
            break;
          }
        }
      }

      if (TunnelingService::Bind(client_, remote_))
        response->code = SOCKS5::SUCCEEDED;
    }

    client_->SendAsync(response, response_length, 0, this);
  } else {
    assert(false);
    DELETE_THIS();
  }
}

void SocksProxySession::OnReceived(AsyncSocket* socket, DWORD error,
                                   void* buffer, int length) {
  if (error != 0 || length == 0) {
    DELETE_THIS();
    return;
  }

  if (request_buffer_[0] == 4) {
    SOCKS4::REQUEST* request =
        reinterpret_cast<SOCKS4::REQUEST*>(request_buffer_.get());

    SOCKS4::RESPONSE* response =
        reinterpret_cast<SOCKS4::RESPONSE*>(response_buffer_.get());
    ::memset(response, 0, sizeof(*response));
    response->code = SOCKS4::FAILED;

    if (request->command == SOCKS4::CONNECT) {
      do {
        remote_.reset(new AsyncSocket());
        if (remote_ == nullptr)
          break;

        if (request->address.s_addr != 0 &&
            ::htonl(request->address.s_addr) <= 0x000000FF) {
          // SOCKS4a extension
          const char* host = request->user_id + ::strlen(request->user_id) + 1;

          auto resolver = new madoka::net::Resolver();
          if (resolver == nullptr)
            break;

          if (!resolver->Resolve(host, ::htons(request->port)))
            break;

          end_point_ = resolver;

          remote_->ConnectAsync(*resolver->begin(), this);
        } else {
          auto address = new SocketAddress4();
          if (address == nullptr)
            break;

          address->ai_socktype = SOCK_STREAM;
          address->sin_port = request->port;
          address->sin_addr = request->address;
          end_point_ = address;

          remote_->ConnectAsync(address, this);
        }

        return;
      } while (false);
    }

    client_->SendAsync(response, sizeof(*response), 0, this);
    return;
  } else if (request_buffer_[0] == 5) {
    if (phase_ == 0) {
      SOCKS5::METHOD_REQUEST* request =
          reinterpret_cast<SOCKS5::METHOD_REQUEST*>(request_buffer_.get());

      SOCKS5::METHOD_RESPONSE* response =
          reinterpret_cast<SOCKS5::METHOD_RESPONSE*>(response_buffer_.get());
      response->version = 5;
      response->method = SOCKS5::UNSUPPORTED;

      for (int i = 0; i < request->method_count; ++i) {
        if (request->methods[i] == SOCKS5::NO_AUTH) {
          response->method = request->methods[i];
          break;
        }
      }

      client_->SendAsync(response, sizeof(*response), 0, this);
      return;
    } else if (phase_ == 1) {
      SOCKS5::REQUEST* request =
          reinterpret_cast<SOCKS5::REQUEST*>(request_buffer_.get());

      SOCKS5::RESPONSE* response =
          reinterpret_cast<SOCKS5::RESPONSE*>(response_buffer_.get());
      ::memset(response, 0, sizeof(*response));
      response->version = 5;
      response->code = SOCKS5::GENERAL_FAILURE;

      if (request->command == SOCKS5::CONNECT) {
        bool connected = false;

        switch (request->type) {
          case SOCKS5::IP_V4:
            connected = ConnectIPv4(request->address);
            break;

          case SOCKS5::DOMAINNAME:
            connected = ConnectDomain(request->address);
            break;

          case SOCKS5::IP_V6:
            connected = ConnectIPv6(request->address);
            break;
        }

        if (connected)
          return;
      }

      client_->SendAsync(response, 10, 0, this);
      return;
    }
  }

  DELETE_THIS();
}

void SocksProxySession::OnSent(AsyncSocket* socket, DWORD error, void* buffer,
                               int length) {
  if (error != 0 || length == 0) {
    DELETE_THIS();
    return;
  }

  if (request_buffer_[0] == 4) {
    SOCKS4::RESPONSE* response =
        reinterpret_cast<SOCKS4::RESPONSE*>(response_buffer_.get());
    if (response->code == SOCKS4::GRANTED) {
      client_ = nullptr;
      remote_ = nullptr;
    }
  } else if (request_buffer_[0] == 5) {
    if (phase_ == 0) {
      ++phase_;
      client_->ReceiveAsync(request_buffer_.get(), kBufferSize, 0, this);
      return;
    } else if (phase_ == 1) {
      SOCKS5::RESPONSE* response =
          reinterpret_cast<SOCKS5::RESPONSE*>(response_buffer_.get());
      if (response->code == SOCKS5::SUCCEEDED) {
        client_ = nullptr;
        remote_ = nullptr;
      }
    }
  } else {
    assert(false);
  }

  DELETE_THIS();
}

bool SocksProxySession::ConnectIPv4(const SOCKS5::ADDRESS& address) {
  auto end_point = std::unique_ptr<SocketAddress4>(new SocketAddress4());
  if (end_point == nullptr)
    return false;

  remote_.reset(new AsyncSocket());
  if (remote_ == nullptr)
    return false;

  end_point->ai_socktype = SOCK_STREAM;
  end_point->sin_port = address.ipv4.ipv4_port;
  end_point->sin_addr = address.ipv4.ipv4_addr;

  end_point_ = end_point.get();

  remote_->ConnectAsync(end_point.release(), this);

  return true;
}

bool SocksProxySession::ConnectDomain(const SOCKS5::ADDRESS& address) {
  std::string domain_name(address.domain.domain_name,
                          address.domain.domain_len);
  uint16_t port = *reinterpret_cast<const uint16_t*>(
      address.domain.domain_name +
      address.domain.domain_len);

  auto resolver = std::unique_ptr<madoka::net::Resolver>(
    new madoka::net::Resolver());
  if (resolver == nullptr)
    return false;

  remote_.reset(new AsyncSocket());
  if (remote_ == nullptr)
    return false;

  if (!resolver->Resolve(domain_name.c_str(), ::htons(port)))
    return false;

  end_point_ = resolver.release();

  remote_->ConnectAsync(*resolver->begin(), this);

  return true;
}

bool SocksProxySession::ConnectIPv6(const SOCKS5::ADDRESS& address) {
  auto end_point = std::unique_ptr<SocketAddress6>(new SocketAddress6());
  if (end_point == nullptr)
    return false;

  remote_.reset(new AsyncSocket());
  if (remote_ == nullptr)
    return false;

  end_point->ai_socktype = SOCK_STREAM;
  end_point->sin6_port = address.ipv6.ipv6_port;
  end_point->sin6_addr = address.ipv6.ipv6_addr;

  end_point_ = end_point.get();

  remote_->ConnectAsync(end_point.release(), this);

  return true;
}

void CALLBACK SocksProxySession::DeleteThis(PTP_CALLBACK_INSTANCE instance,
                                            void* param) {
  delete static_cast<SocksProxySession*>(param);
}
