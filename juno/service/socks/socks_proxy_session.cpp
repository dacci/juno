// Copyright (c) 2013 dacci.org

#include "service/socks/socks_proxy_session.h"

#include <assert.h>

#include <string>

#include "net/socket_resolver.h"
#include "net/tunneling_service.h"
#include "service/socks/socks_proxy.h"

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

SocksProxySession::SocksProxySession(SocksProxy* proxy,
                                     const Service::ChannelPtr& client)
    : proxy_(proxy),
      client_(client),
      phase_(),
      end_point_() {
}

SocksProxySession::~SocksProxySession() {
  Stop();
}

bool SocksProxySession::Start() {
  remote_ = std::make_shared<SocketChannel>();
  if (remote_ == nullptr)
    return false;

  auto result = client_->ReadAsync(request_buffer_, kBufferSize, this);
  if (FAILED(result))
    return false;

  return true;
}

void SocksProxySession::Stop() {
  if (client_ != nullptr)
    client_->Close();

  if (remote_ != nullptr) {
    if (remote_->connected())
      remote_->Shutdown(SD_BOTH);
    else
      remote_->Close();
  }
}

void SocksProxySession::OnConnected(SocketChannel* channel, HRESULT result) {
  if (request_buffer_[0] == 4) {
    SOCKS4::REQUEST* request =
        reinterpret_cast<SOCKS4::REQUEST*>(request_buffer_);

    if (request->address.s_addr != 0 &&
        ::htonl(request->address.s_addr) <= 0x000000FF)
      delete static_cast<SocketResolver*>(end_point_);
    else
      delete static_cast<SocketAddress4*>(end_point_);

    SOCKS4::RESPONSE* response =
        reinterpret_cast<SOCKS4::RESPONSE*>(response_buffer_);

    if (SUCCEEDED(result) && TunnelingService::Bind(client_, remote_))
      response->code = SOCKS4::GRANTED;

    result = client_->WriteAsync(response, sizeof(*response), this);
    if (FAILED(result))
      proxy_->EndSession(this);
  } else if (request_buffer_[0] == 5) {
    SOCKS5::REQUEST* request =
        reinterpret_cast<SOCKS5::REQUEST*>(request_buffer_);

    SOCKS5::RESPONSE* response =
        reinterpret_cast<SOCKS5::RESPONSE*>(response_buffer_);
    int response_length = 10;

    switch (request->type) {
      case SOCKS5::IP_V4:
        delete static_cast<SocketAddress4*>(end_point_);
        break;

      case SOCKS5::DOMAINNAME:
        delete static_cast<SocketResolver*>(end_point_);
        break;

      case SOCKS5::IP_V6:
        delete static_cast<SocketAddress6*>(end_point_);
        break;
    }

    if (SUCCEEDED(result)) {
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

    result = client_->WriteAsync(response, response_length, this);
    if (FAILED(result))
      proxy_->EndSession(this);
  } else {
    assert(false);
    proxy_->EndSession(this);
  }
}

void SocksProxySession::OnRead(Channel* channel, HRESULT result, void* buffer,
                               int length) {
  if (FAILED(result) || length == 0) {
    proxy_->EndSession(this);
    return;
  }

  if (request_buffer_[0] == 4) {
    SOCKS4::REQUEST* request =
        reinterpret_cast<SOCKS4::REQUEST*>(request_buffer_);

    SOCKS4::RESPONSE* response =
        reinterpret_cast<SOCKS4::RESPONSE*>(response_buffer_);
    ::memset(response, 0, sizeof(*response));
    response->code = SOCKS4::FAILED;

    if (request->command == SOCKS4::CONNECT) {
      do {
        if (request->address.s_addr != 0 &&
            ::htonl(request->address.s_addr) <= 0x000000FF) {
          // SOCKS4a extension
          const char* host = request->user_id + ::strlen(request->user_id) + 1;

          auto resolver = std::make_unique<SocketResolver>();
          if (resolver == nullptr)
            break;

          auto result = resolver->Resolve(host, ::htons(request->port));
          if (FAILED(result))
            break;

          end_point_ = resolver.get();
          remote_->ConnectAsync(resolver->begin()->get(), this);
          resolver.release();
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

    result = client_->WriteAsync(response, sizeof(*response), this);
    if (SUCCEEDED(result))
      return;
  } else if (request_buffer_[0] == 5) {
    if (phase_ == 0) {
      SOCKS5::METHOD_REQUEST* request =
          reinterpret_cast<SOCKS5::METHOD_REQUEST*>(request_buffer_);

      SOCKS5::METHOD_RESPONSE* response =
          reinterpret_cast<SOCKS5::METHOD_RESPONSE*>(response_buffer_);
      response->version = 5;
      response->method = SOCKS5::UNSUPPORTED;

      for (int i = 0; i < request->method_count; ++i) {
        if (request->methods[i] == SOCKS5::NO_AUTH) {
          response->method = request->methods[i];
          break;
        }
      }

      result = client_->WriteAsync(response, sizeof(*response), this);
      if (SUCCEEDED(result))
        return;
    } else if (phase_ == 1) {
      SOCKS5::REQUEST* request =
          reinterpret_cast<SOCKS5::REQUEST*>(request_buffer_);

      SOCKS5::RESPONSE* response =
          reinterpret_cast<SOCKS5::RESPONSE*>(response_buffer_);
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

      result = client_->WriteAsync(response, 10, this);
      if (SUCCEEDED(result))
        return;
    }
  }

  proxy_->EndSession(this);
}

void SocksProxySession::OnWritten(Channel* channel, HRESULT result,
                                  void* buffer, int length) {
  if (FAILED(result) || length == 0) {
    proxy_->EndSession(this);
    return;
  }

  if (request_buffer_[0] == 4) {
    SOCKS4::RESPONSE* response =
        reinterpret_cast<SOCKS4::RESPONSE*>(response_buffer_);
    if (response->code == SOCKS4::GRANTED) {
      client_.reset();
      remote_.reset();
    }
  } else if (request_buffer_[0] == 5) {
    if (phase_ == 0) {
      ++phase_;
      result = client_->ReadAsync(request_buffer_, kBufferSize, this);
      if (SUCCEEDED(result))
        return;
    } else if (phase_ == 1) {
      SOCKS5::RESPONSE* response =
          reinterpret_cast<SOCKS5::RESPONSE*>(response_buffer_);
      if (response->code == SOCKS5::SUCCEEDED) {
        client_.reset();
        remote_.reset();
      }
    }
  } else {
    assert(false);
  }

  proxy_->EndSession(this);
}

bool SocksProxySession::ConnectIPv4(const SOCKS5::ADDRESS& address) {
  auto end_point = std::make_unique<SocketAddress4>();
  if (end_point == nullptr)
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

  auto resolver = std::make_unique<SocketResolver>();
  if (resolver == nullptr)
    return false;

  auto result = resolver->Resolve(domain_name, ::htons(port));
  if (FAILED(result))
    return false;

  remote_->ConnectAsync(resolver->begin()->get(), this);

  end_point_ = resolver.release();

  return true;
}

bool SocksProxySession::ConnectIPv6(const SOCKS5::ADDRESS& address) {
  auto end_point = std::make_unique<SocketAddress6>();
  if (end_point == nullptr)
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
