// Copyright (c) 2016 dacci.org

#ifndef JUNO_NET_SOCKET_RESOLVER_H_
#define JUNO_NET_SOCKET_RESOLVER_H_

#include <winerror.h>
#include <ws2tcpip.h>

#include <memory>
#include <string>
#include <vector>

class SocketAddress : public addrinfo {
 public:
  explicit SocketAddress(const addrinfo& end_point);

 private:
  sockaddr_storage sockaddr_;

  SocketAddress(const SocketAddress&) = delete;
  SocketAddress& operator=(const SocketAddress&) = delete;
};

class SocketResolver {
 public:
  SocketResolver();

  HRESULT Resolve(const char* node_name, const char* service);
  HRESULT Resolve(const std::string& node_name, const std::string& service) {
    return Resolve(node_name.c_str(), service.c_str());
  }

  HRESULT Resolve(const char* node_name, int port);
  HRESULT Resolve(const std::string& node_name, int port) {
    return Resolve(node_name.c_str(), port);
  }

  auto begin() const {
    return end_points_.begin();
  }

  auto end() const {
    return end_points_.end();
  }

  int GetFlags() const {
    return hints_.ai_flags;
  }

  void SetFlags(int flags) {
    hints_.ai_flags = flags;
  }

  int GetFamily() const {
    return hints_.ai_family;
  }

  void SetFamily(int family) {
    hints_.ai_family = family;
  }

  int GetType() const {
    return hints_.ai_socktype;
  }

  void SetType(int type) {
    hints_.ai_socktype = type;
  }

  int GetProtocol() const {
    return hints_.ai_protocol;
  }

  void SetProtocol(int protocol) {
    hints_.ai_protocol = protocol;
  }

 private:
  typedef std::vector<std::unique_ptr<SocketAddress>> AddressList;

  addrinfo hints_;
  AddressList end_points_;

  SocketResolver(const SocketResolver&) = delete;
  SocketResolver& operator=(const SocketResolver&) = delete;
};

#endif  // JUNO_NET_SOCKET_RESOLVER_H_
