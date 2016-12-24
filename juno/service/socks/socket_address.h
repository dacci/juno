// Copyright (c) 2016 dacci.org

#ifndef JUNO_SERVICE_SOCKS_SOCKET_ADDRESS_H_
#define JUNO_SERVICE_SOCKS_SOCKET_ADDRESS_H_

#include <ws2tcpip.h>

namespace {

struct SocketAddress4 : addrinfo, sockaddr_in {
  SocketAddress4() : addrinfo(), sockaddr_in() {
    ai_family = AF_INET;
    ai_addrlen = sizeof(sockaddr_in);
    ai_addr = reinterpret_cast<sockaddr*>(static_cast<sockaddr_in*>(this));

    sin_family = static_cast<ADDRESS_FAMILY>(ai_family);
  }

  operator sockaddr*() const {
    return ai_addr;
  }
};

struct SocketAddress6 : addrinfo, sockaddr_in6 {
  SocketAddress6() : addrinfo(), sockaddr_in6() {
    ai_family = AF_INET6;
    ai_addrlen = sizeof(sockaddr_in6);
    ai_addr = reinterpret_cast<sockaddr*>(static_cast<sockaddr_in6*>(this));

    sin6_family = static_cast<ADDRESS_FAMILY>(ai_family);
  }

  operator sockaddr*() const {
    return ai_addr;
  }
};

}  // namespace

#endif  // JUNO_SERVICE_SOCKS_SOCKET_ADDRESS_H_
