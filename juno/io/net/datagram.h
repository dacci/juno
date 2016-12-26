// Copyright (c) 2015 dacci.org

#ifndef JUNO_IO_NET_DATAGRAM_H_
#define JUNO_IO_NET_DATAGRAM_H_

#include <ws2tcpip.h>

#include <memory>

namespace juno {
namespace io {
namespace net {

class DatagramChannel;

struct Datagram {
  std::shared_ptr<DatagramChannel> channel;
  int data_length;
  std::unique_ptr<char[]> data;
  int from_length;
  sockaddr_storage from;
};

#ifndef JUNO_NO_DATAGRAM_PTR
typedef std::shared_ptr<Datagram> DatagramPtr;
#endif  // JUNO_NO_DATAGRAM_PTR

}  // namespace net
}  // namespace io
}  // namespace juno

#endif  // JUNO_IO_NET_DATAGRAM_H_
