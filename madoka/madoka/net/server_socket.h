// Copyright (c) 2013 dacci.org

#ifndef MADOKA_NET_SERVER_SOCKET_H_
#define MADOKA_NET_SERVER_SOCKET_H_

#include <madoka/net/abstract_socket.h>
#include <madoka/net/address_info.h>
#include <madoka/net/socket.h>

namespace madoka {
namespace net {

class ServerSocket : public AbstractSocket {
 public:
  ServerSocket() : bound_(), address_length_(0) {
  }

  virtual ~ServerSocket() {
    Close();
  }

  virtual void Close() {
    bound_ = false;
    AbstractSocket::Close();
  }

  bool Bind(const sockaddr* address, size_t length) {
    if (bound_)
      return false;
    if (!IsValid())
      return false;

    address_length_ = length;
    bound_ = ::bind(descriptor_, address, static_cast<int>(length)) == 0;
    return bound_;
  }

  bool Bind(const addrinfo* end_point) {
    return Bind(end_point->ai_addr, end_point->ai_addrlen);
  }

  virtual bool Bind(const char* node_name, const char* service) {
    if (bound_)
      return false;

    AddressInfo info;
    info.ai_flags = AI_PASSIVE;
    info.ai_socktype = SOCK_STREAM;
    if (!info.Resolve(node_name, service))
      return false;

    for (AddressInfo::iterator i = info.begin(), l = info.end(); i != l; ++i) {
      if (Create(*i) && Bind(*i))
        break;

      Close();
    }

    return bound_;
  }

  virtual bool Bind(const char* node_name, int port) {
    if (!IsValidPort(port))
      return false;

    char service[8];
#ifdef _MSC_VER
    ::sprintf_s(service, "%d", port);
#else
    ::snprintf(service, sizeof(service), "%d", port);
#endif  // _MSC_VER

    return Bind(node_name, service);
  }

#ifdef WIN32
  bool Bind(const ADDRINFOW* end_point) {
    return Bind(end_point->ai_addr, end_point->ai_addrlen);
  }

  virtual bool Bind(const wchar_t* node_name, const wchar_t* service) {
    if (bound_)
      return false;

    AddressInfoW info;
    info.ai_flags = AI_PASSIVE;
    info.ai_socktype = SOCK_STREAM;
    if (!info.Resolve(node_name, service))
      return false;

    for (AddressInfoW::iterator i = info.begin(), l = info.end(); i != l; ++i) {
      if (Create(*i) && Bind(*i))
        break;

      Close();
    }

    return bound_;
  }

  virtual bool Bind(const wchar_t* node_name, int port) {
    if (!IsValidPort(port))
      return false;

    wchar_t service[8];
#ifdef _MSC_VER
    ::swprintf_s(service, L"%d", port);
#else
    ::swprintf(service, _countof(service), L"%d", port);
#endif  // _MSC_VER

    return Bind(node_name, service);
  }
#endif  // WIN32

  bool Listen(int backlog) {
    if (!bound_)
      return false;

    return ::listen(descriptor_, backlog) == 0;
  }

  virtual Socket* Accept() {
    if (!bound_)
      return NULL;

    int length = address_length_;
    sockaddr* address = static_cast<sockaddr*>(::malloc(length));

    SOCKET peer = ::accept(descriptor_, address, &length);
    ::free(address);

    if (peer == INVALID_SOCKET)
      return NULL;

    return new Socket(peer);
  }

 private:
  bool bound_;
  size_t address_length_;

  DISALLOW_COPY_AND_ASSIGN(ServerSocket);
};

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_SERVER_SOCKET_H_
