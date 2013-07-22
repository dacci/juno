// Copyright (c) 2013 dacci.org

#ifndef MADOKA_NET_ABSTRACT_SOCKET_H_
#define MADOKA_NET_ABSTRACT_SOCKET_H_

#include <limits.h>

#include <madoka/net/common.h>

namespace madoka {
namespace net {

class AbstractSocket {
 public:
  virtual ~AbstractSocket() {
    Close();
  }

  virtual void Close() {
    if (IsValid()) {
      ::closesocket(descriptor_);
      descriptor_ = INVALID_SOCKET;
    }
  }

  bool GetOption(int level, int name, void* value, int* length) {
    if (!IsValid())
      return false;

    return ::getsockopt(descriptor_, level, name, static_cast<char*>(value),
                        length) == 0;
  }

  bool SetOption(int level, int name, const void* value, int length) {
    if (!IsValid())
      return false;

    return ::setsockopt(descriptor_, level, name,
                        static_cast<const char*>(value), length) == 0;
  }

  template<typename T>
  bool SetOption(int level, int name, const T& value) {
    return SetOption(level, name, &value, sizeof(value));
  }

  bool IOControl(int command, u_long* param) {
    if (!IsValid())
      return false;

    return ::ioctlsocket(descriptor_, command, param) == 0;
  }

  bool GetLocalEndPoint(sockaddr* end_point, int* length) {
    if (!IsValid())
      return false;

    return ::getsockname(descriptor_, end_point, length) == 0;
  }

  bool IsValid() const {
    return descriptor_ != INVALID_SOCKET;
  }

  static bool IsValidPort(int port) {
    if (0 <= port && port <= USHRT_MAX)
      return true;
    else
      return false;
  }

  operator SOCKET() const {
    return descriptor_;
  }

 protected:
  AbstractSocket() : descriptor_(INVALID_SOCKET) {
  }

  explicit AbstractSocket(SOCKET descriptor) : descriptor_(descriptor) {
  }

  bool Create(int family, int type, int protocol) {
    Close();

    descriptor_ = ::socket(family, type, protocol);
    return IsValid();
  }

  bool Create(const addrinfo* end_point) {
    return Create(end_point->ai_family, end_point->ai_socktype,
                  end_point->ai_protocol);
  }

#ifdef WIN32
  bool Create(const ADDRINFOW* end_point) {
    return Create(end_point->ai_family, end_point->ai_socktype,
                  end_point->ai_protocol);
  }
#endif  // WIN32

  SOCKET descriptor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AbstractSocket);
};

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_ABSTRACT_SOCKET_H_
