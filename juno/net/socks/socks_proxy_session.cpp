// Copyright (c) 2013 dacci.org

#include "net/socks/socks_proxy_session.h"

#include <assert.h>

#include <madoka/net/address_info.h>

#include <string>

#include "net/tunneling_service.h"
#include "net/socks/socks_proxy.h"

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

#ifdef LEGACY_PLATFORM
  #define DELETE_THIS() \
    delete this
#else   // LEGACY_PLATFORM
  #define DELETE_THIS() \
    ::TrySubmitThreadpoolCallback(DeleteThis, this, NULL)
#endif  // LEGACY_PLATFORM

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

  if (request_buffer_ != NULL) {
    delete[] request_buffer_;
    request_buffer_ = NULL;
  }

  if (response_buffer_ != NULL) {
    delete[] response_buffer_;
    response_buffer_ = NULL;
  }

  proxy_->EndSession(this);
}

bool SocksProxySession::Start(AsyncSocket* client) {
  client_ = client;

  if (!client_->ReceiveAsync(request_buffer_, kBufferSize, 0, this)) {
    client_ = NULL;
    return false;
  }

  return true;
}

void SocksProxySession::Stop() {
  if (client_ != NULL)
    client_->Shutdown(SD_BOTH);

  if (remote_ != NULL)
    remote_->Shutdown(SD_BOTH);
}

void SocksProxySession::OnConnected(AsyncSocket* socket, DWORD error) {
  if (request_buffer_[0] == 4) {
    delete static_cast<SocketAddress4*>(end_point_);

    SOCKS4::RESPONSE* response =
        reinterpret_cast<SOCKS4::RESPONSE*>(response_buffer_);

    if (error == 0 && TunnelingService::Bind(client_, remote_))
        response->code = SOCKS4::GRANTED;

    if (client_->SendAsync(response, sizeof(*response), 0, this))
      return;
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
        delete static_cast<madoka::net::AddressInfo*>(end_point_);
        break;

      case SOCKS5::IP_V6:
        delete static_cast<SocketAddress6*>(end_point_);
        break;
    }

    if (error == 0) {
      int length = 32;
      sockaddr* address = static_cast<sockaddr*>(::malloc(length));

      if (remote_->GetLocalEndPoint(address, &length)) {
        switch (address->sa_family) {
          case AF_INET: {
            sockaddr_in* address4 = reinterpret_cast<sockaddr_in*>(address);
            response->type = SOCKS5::IP_V4;
            response->address.ipv4.ipv4_addr = address4->sin_addr;
            response->address.ipv4.ipv4_port = address4->sin_port;
            break;
          }

          case AF_INET6: {
            sockaddr_in6* address6 = reinterpret_cast<sockaddr_in6*>(address);
            response->type = SOCKS5::IP_V6;
            response->address.ipv6.ipv6_addr = address6->sin6_addr;
            response->address.ipv6.ipv6_port = address6->sin6_port;
            response_length += 12;
            break;
          }
        }
      }

      ::free(address);

      if (TunnelingService::Bind(client_, remote_))
        response->code = SOCKS5::SUCCEEDED;
    }

    if (client_->SendAsync(response, response_length, 0, this))
      return;
  } else {
    assert(false);
  }

  DELETE_THIS();
}

void SocksProxySession::OnReceived(AsyncSocket* socket, DWORD error,
                                   int length) {
  if (error != 0 || length == 0) {
    DELETE_THIS();
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
        remote_ = new AsyncSocket();
        if (remote_ == NULL)
          break;

        SocketAddress4* address = new SocketAddress4();
        if (address == NULL)
          break;

        address->ai_socktype = SOCK_STREAM;
        address->sin_port = request->port;
        address->sin_addr = request->address;
        end_point_ = address;
        if (!remote_->ConnectAsync(end_point_, this))
          break;

        return;
      } while (false);
    }

    if (client_->SendAsync(response, sizeof(*response), 0, this))
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

      if (client_->SendAsync(response, sizeof(*response), 0, this))
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

      if (client_->SendAsync(response, 10, 0, this))
        return;
    }
  }

  DELETE_THIS();
}

void SocksProxySession::OnSent(AsyncSocket* socket, DWORD error, int length) {
  if (error != 0 || length == 0) {
    DELETE_THIS();
    return;
  }

  if (request_buffer_[0] == 4) {
    SOCKS4::RESPONSE* response =
        reinterpret_cast<SOCKS4::RESPONSE*>(response_buffer_);
    if (response->code == SOCKS4::GRANTED) {
      client_ = NULL;
      remote_ = NULL;
    }
  } else if (request_buffer_[0] == 5) {
    if (phase_ == 0) {
      ++phase_;
      if (client_->ReceiveAsync(request_buffer_, kBufferSize, 0, this))
        return;
    } else if (phase_ == 1) {
      SOCKS5::RESPONSE* response =
          reinterpret_cast<SOCKS5::RESPONSE*>(response_buffer_);
      if (response->code == SOCKS5::SUCCEEDED) {
        client_ = NULL;
        remote_ = NULL;
      }
    }
  } else {
    assert(false);
  }

  DELETE_THIS();
}

bool SocksProxySession::ConnectIPv4(const SOCKS5::ADDRESS& address) {
  SocketAddress4* end_point = NULL;

  do {
    end_point = new SocketAddress4;
    if (end_point == NULL)
      break;

    remote_ = new AsyncSocket();
    if (remote_ == NULL)
      break;

    end_point->ai_socktype = SOCK_STREAM;
    end_point->sin_port = address.ipv4.ipv4_port;
    end_point->sin_addr = address.ipv4.ipv4_addr;

    end_point_ = end_point;

    if (!remote_->ConnectAsync(end_point_, this))
      break;

    return true;
  } while (false);

  delete end_point;

  return false;
}

bool SocksProxySession::ConnectDomain(const SOCKS5::ADDRESS& address) {
  std::string domain_name(address.domain.domain_name,
                          address.domain.domain_len);
  uint16_t port = *reinterpret_cast<const uint16_t*>(
      address.domain.domain_name +
      address.domain.domain_len);

  madoka::net::AddressInfo* resolver = NULL;

  do {
    resolver = new madoka::net::AddressInfo();
    if (resolver == NULL)
      break;

    remote_ = new AsyncSocket();
    if (remote_ == NULL)
      break;

    if (!resolver->Resolve(domain_name.c_str(), ::htons(port)))
      break;

    end_point_ = resolver;

    if (!remote_->ConnectAsync(**resolver, this))
      break;

    return true;
  } while (false);

  delete resolver;

  return false;
}

bool SocksProxySession::ConnectIPv6(const SOCKS5::ADDRESS& address) {
  SocketAddress6* end_point = NULL;

  do {
    end_point = new SocketAddress6;
    if (end_point == NULL)
      break;

    remote_ = new AsyncSocket();
    if (remote_ == NULL)
      break;

    end_point->ai_socktype = SOCK_STREAM;
    end_point->sin6_port = address.ipv6.ipv6_port;
    end_point->sin6_addr = address.ipv6.ipv6_addr;

    end_point_ = end_point;

    if (!remote_->ConnectAsync(end_point_, this))
      break;

    return true;
  } while (false);

  delete end_point;

  return false;
}

void CALLBACK SocksProxySession::DeleteThis(PTP_CALLBACK_INSTANCE instance,
                                            void* param) {
  delete static_cast<SocksProxySession*>(param);
}
