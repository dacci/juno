// Copyright (c) 2016 dacci.org

#ifndef JUNO_NET_ABSTRACT_SOCKET_H_
#define JUNO_NET_ABSTRACT_SOCKET_H_

#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

class AbstractSocket {
 public:
  bool Create(int family, int type, int protocol) {
    if (IsValid())
      Close();

    descriptor_ = socket(family, type, protocol);

    return IsValid();
  }

  bool Bind(const void* address, size_t length) {
    if (!IsValid() || bound_)
      return false;

    if (bind(descriptor_, static_cast<const sockaddr*>(address),
             static_cast<int>(length)) != 0)
      return false;

    bound_ = true;

    return true;
  }

  bool Bind(const addrinfo* end_point) {
    if (bound_)
      return false;

    if (!Create(end_point->ai_family, end_point->ai_socktype,
                end_point->ai_protocol))
      return false;

    return Bind(end_point->ai_addr, end_point->ai_addrlen);
  }

  bool IOControl(int command, DWORD* param) {
    return ioctlsocket(descriptor_, command, param) == 0;
  }

  bool GetLocalEndPoint(void* address, int* length) const {
    return getsockname(descriptor_, static_cast<sockaddr*>(address), length) ==
           0;
  }

  bool GetOption(int level, int option, void* value, int* length) const {
    return getsockopt(descriptor_, level, option, static_cast<char*>(value),
                      length) == 0;
  }

  template <class T>
  bool GetOption(int level, int option, T* value) const {
    int length = sizeof(*value);
    return GetOption(level, option, value, &length);
  }

  bool SetOption(int level, int option, const void* value, size_t length) {
    return setsockopt(descriptor_, level, option,
                      static_cast<const char*>(value),
                      static_cast<int>(length)) == 0;
  }

  template <class T>
  bool SetOption(int level, int option, const T& value) {
    return SetOption(level, option, value, sizeof(value));
  }

  bool IsValid() const {
    return descriptor_ != INVALID_SOCKET;
  }

  bool bound() const {
    return bound_;
  }

 protected:
  AbstractSocket() : descriptor_(INVALID_SOCKET), bound_(false) {}

  virtual ~AbstractSocket() {
    Close();
  }

  virtual void Close() {
    closesocket(descriptor_);
    descriptor_ = INVALID_SOCKET;
    bound_ = false;
  }

  SOCKET descriptor_;
  bool bound_;

 private:
  AbstractSocket(const AbstractSocket&) = delete;
  AbstractSocket& operator=(const AbstractSocket&) = delete;
};

#endif  // JUNO_NET_ABSTRACT_SOCKET_H_
