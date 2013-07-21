// Copyright (c) 2013 dacci.org

#ifndef MADOKA_NET_DATAGRAM_SOCKET_H_
#define MADOKA_NET_DATAGRAM_SOCKET_H_

#include <stdio.h>

#include <madoka/net/abstract_socket.h>

namespace madoka {
namespace net {

class DatagramSocket : public AbstractSocket {
 public:
  DatagramSocket() : bound_(), connected_() {
  }

  virtual ~DatagramSocket() {
    Close();
  }

  virtual void Close() {
    bound_ = false;
    connected_ = false;
    AbstractSocket::Close();
  }

  bool Bind(const sockaddr* address, size_t length) {
    if (bound_)
      return false;
    if (!IsValid())
      return false;

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
    info.ai_socktype = SOCK_DGRAM;
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

  bool Connect(const sockaddr* address, size_t length) {
    if (connected_)
      return false;
    if (!IsValid())
      return false;

    connected_ = ::connect(descriptor_, address, static_cast<int>(length)) == 0;
    return connected_;
  }

  bool Connect(const addrinfo* end_point) {
    return Connect(end_point->ai_addr, end_point->ai_addrlen);
  }

  virtual bool Connect(const char* node_name, const char* service) {
    if (connected_)
      return false;

    AddressInfo info;
    info.ai_socktype = SOCK_DGRAM;
    if (!info.Resolve(node_name, service))
      return false;

    for (AddressInfo::iterator i = info.begin(), l = info.end(); i != l; ++i) {
      if (Create(*i) && Connect(*i))
        break;

      Close();
    }

    return connected_;
  }

  virtual bool Connect(const char* node_name, int port) {
    if (!IsValidPort(port))
      return false;

    char service[8];
#ifdef _MSC_VER
    ::sprintf_s(service, "%d", port);
#else
    ::snprintf(service, sizeof(service), "%d", port);
#endif  // _MSC_VER

    return Connect(node_name, service);
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
    info.ai_socktype = SOCK_DGRAM;
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

  bool Connect(const ADDRINFOW* end_point) {
    return Connect(end_point->ai_addr, end_point->ai_addrlen);
  }

  virtual bool Connect(const wchar_t* node_name, const wchar_t* service) {
    if (bound_ || connected_)
      return false;

    AddressInfoW info;
    info.ai_socktype = SOCK_DGRAM;
    if (!info.Resolve(node_name, service))
      return false;

    for (AddressInfoW::iterator i = info.begin(), l = info.end(); i != l; ++i) {
      if (Create(*i) && Connect(*i))
        break;

      Close();
    }

    return connected_;
  }

  virtual bool Connect(const wchar_t* node_name, int port) {
    if (!IsValidPort(port))
      return false;

    wchar_t service[8];
#ifdef _MSC_VER
    ::swprintf_s(service, L"%d", port);
#else
    ::swprintf(service, _countof(service), L"%d", port);
#endif  // _MSC_VER

    return Connect(node_name, service);
  }
#endif  // WIN32

  virtual int Receive(void* buffer, int length, int flags) {
    if (!IsValid() || !(bound_ || connected_) || buffer == NULL || length < 0)
      return SOCKET_ERROR;
    if (length == 0)
      return 0;

    return ::recv(descriptor_, static_cast<char*>(buffer), length, flags);
  }

  virtual int Send(const void* buffer, int length, int flags) {
    if (!IsValid() || !connected_ || buffer == NULL || length < 0)
      return SOCKET_ERROR;
    if (length == 0)
      return 0;

    return ::send(descriptor_, static_cast<const char*>(buffer), length,
                  flags);
  }

  int ReceiveFrom(void* buffer, int length, int flags, sockaddr* from,
                  int* fromlen) {
    if (!IsValid() || buffer == NULL || length < 0)
      return SOCKET_ERROR;

    return ::recvfrom(descriptor_, static_cast<char*>(buffer), length, flags,
                      from, fromlen);
  }

  int SendTo(const void* buffer, int length, int flags, const sockaddr* to,
             int tolen) {
    if (!IsValid() || buffer == NULL || length < 0)
      return SOCKET_ERROR;

    return ::sendto(descriptor_, static_cast<const char*>(buffer), length,
                    flags, to, tolen);
  }

 private:
  bool bound_;
  bool connected_;

  DISALLOW_COPY_AND_ASSIGN(DatagramSocket);
};

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_DATAGRAM_SOCKET_H_
