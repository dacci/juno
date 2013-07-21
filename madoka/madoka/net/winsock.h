// Copyright (c) 2013 dacci.org

#ifndef MADOKA_NET_WINSOCK_H_
#define MADOKA_NET_WINSOCK_H_

#include <madoka/net/common.h>

namespace madoka {
namespace net {

class WinSock : public WSADATA {
 public:
  WinSock() : WSADATA(), error_(WSANOTINITIALISED) {
  }

  explicit WinSock(WORD version) : WSADATA(), error_(WSANOTINITIALISED) {
    Startup(version);
  }

  ~WinSock() {
    if (Initialized())
      Cleanup();
  }

  int Startup(WORD version) {
    error_ = ::WSAStartup(version, this);
    return error_;
  }

  int Cleanup() {
    error_ = WSANOTINITIALISED;
    return ::WSACleanup();
  }

  int error() const {
    return error_;
  }

  bool Initialized() const {
    return error_ == 0;
  }

  operator bool() const {
    return Initialized();
  }

 private:
  int error_;

  DISALLOW_COPY_AND_ASSIGN(WinSock);
};

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_WINSOCK_H_
