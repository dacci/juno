// Copyright (c) 2016 dacci.org

#ifndef JUNO_IO_NET_SOCKET_H_
#define JUNO_IO_NET_SOCKET_H_

#include "io/net/abstract_socket.h"

class Socket : public AbstractSocket {
 public:
  Socket() : connected_(false) {}

  virtual ~Socket() {
    Close();
  }

  void Close() override {
    AbstractSocket::Close();
    connected_ = false;
  }

  bool Connect(const void* address, int length) {
    if (!IsValid() || connected_)
      return false;

    if (connect(descriptor_, static_cast<const sockaddr*>(address), length) !=
        0)
      return false;

    bound_ = true;
    connected_ = true;

    return true;
  }

  bool Connect(const addrinfo* end_point) {
    if (connected_)
      return false;

    if (!Create(end_point))
      return false;

    return Connect(end_point->ai_addr, static_cast<int>(end_point->ai_addrlen));
  }

  bool Shutdown(int how) {
    if (shutdown(descriptor_, how) != 0)
      return false;

    connected_ = false;

    return true;
  }

  int Receive(void* buffer, int length, int flags) {
    return recv(descriptor_, static_cast<char*>(buffer), length, flags);
  }

  int ReceiveFrom(void* buffer, int length, int flags, void* from,
                  int* from_length) {
    return recvfrom(descriptor_, static_cast<char*>(buffer), length, flags,
                    static_cast<sockaddr*>(from), from_length);
  }

  int Send(const void* buffer, int length, int flags) {
    return send(descriptor_, static_cast<const char*>(buffer), length, flags);
  }

  int SendTo(const void* buffer, int length, int flags, const void* to,
             int to_length) {
    return sendto(descriptor_, static_cast<const char*>(buffer), length, flags,
                  static_cast<const sockaddr*>(to), to_length);
  }

  bool GetRemoteEndPoint(void* address, int* length) const {
    return getpeername(descriptor_, static_cast<sockaddr*>(address), length) ==
           0;
  }

  bool connected() const {
    return connected_;
  }

 protected:
  bool connected_;

 private:
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;
};

#endif  // JUNO_IO_NET_SOCKET_H_
