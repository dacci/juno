// Copyright (c) 2016 dacci.org

#ifndef JUNO_IO_NET_SERVER_SOCKET_H_
#define JUNO_IO_NET_SERVER_SOCKET_H_

#include <memory>
#include <type_traits>

#include "io/net/abstract_socket.h"
#include "io/net/socket.h"

namespace juno {
namespace io {
namespace net {

class ServerSocket : public AbstractSocket {
 public:
  ServerSocket() : listening_(false) {}

  virtual ~ServerSocket() {
    ServerSocket::Close();
  }

  void Close() override {
    AbstractSocket::Close();
    listening_ = false;
  }

  bool Listen(int backlog) {
    if (!IsValid() || listening_)
      return false;

    if (listen(descriptor_, backlog) != 0)
      return false;

    listening_ = true;

    return true;
  }

  template <class T = Socket>
  std::unique_ptr<T> Accept() {
    static_assert(std::is_base_of<Socket, T>::value,
                  "T must be a descendant of Socket");

    struct Wrapper : T {
      explicit Wrapper(SOCKET descriptor) {
        descriptor_ = descriptor;
        bound_ = true;
        connected_ = true;
      }
    };

    if (!IsValid() || !listening_)
      return nullptr;

    sockaddr_storage address;
    int length = sizeof(address);
    auto peer_descriptor =
        accept(descriptor_, reinterpret_cast<sockaddr*>(&address), &length);
    if (peer_descriptor == INVALID_SOCKET)
      return nullptr;

    auto peer = std::make_unique<Wrapper>(peer_descriptor);
    if (peer == nullptr) {
      closesocket(peer_descriptor);
      return nullptr;
    }

    return std::move(peer);
  }

  bool listening() const {
    return listening_;
  }

 protected:
  bool listening_;

 private:
  ServerSocket(const ServerSocket&) = delete;
  ServerSocket& operator=(const ServerSocket&) = delete;
};

}  // namespace net
}  // namespace io
}  // namespace juno

#endif  // JUNO_IO_NET_SERVER_SOCKET_H_
