// Copyright (c) 2015 dacci.org

#ifndef JUNO_NET_DATAGRAM_H_
#define JUNO_NET_DATAGRAM_H_

#include <madoka/net/async_socket.h>

#include <memory>

struct Datagram {
  std::shared_ptr<madoka::net::AsyncSocket> socket;
  int data_length;
  std::unique_ptr<char[]> data;
  int from_length;
  sockaddr_storage from;
};

#endif  // JUNO_NET_DATAGRAM_H_
