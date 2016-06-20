// Copyright (c) 2016 dacci.org

#include "net/socket_resolver.h"

#include <stdio.h>

SocketAddress::SocketAddress(const addrinfo& end_point)
    : addrinfo(end_point), sockaddr_() {
  ai_canonname = nullptr;
  ai_addr = reinterpret_cast<sockaddr*>(&sockaddr_);
  ai_next = nullptr;

  memcpy(&sockaddr_, end_point.ai_addr, end_point.ai_addrlen);
}

SocketResolver::SocketResolver() : hints_() {
  SetType(SOCK_STREAM);
}

HRESULT SocketResolver::Resolve(const char* node_name, const char* service) {
  addrinfo* resolved = nullptr;
  auto error = getaddrinfo(node_name, service, &hints_, &resolved);
  if (error != 0)
    return HRESULT_FROM_WIN32(error);

  AddressList new_list;
  for (auto end_point = resolved; end_point; end_point = end_point->ai_next)
    new_list.push_back(std::make_unique<SocketAddress>(*end_point));

  freeaddrinfo(resolved);

  addrinfo* next_end_point = nullptr;
  for (auto i = new_list.rbegin(), l = new_list.rend(); i != l; ++i) {
    (*i)->ai_next = next_end_point;
    next_end_point = i->get();
  }

  end_points_ = std::move(new_list);

  return S_OK;
}

HRESULT SocketResolver::Resolve(const char* node_name, int port) {
  if (port < 0 || 65535 < port)
    return E_INVALIDARG;

  char service[8];
  sprintf_s(service, "%d", port);

  return Resolve(node_name, service);
}
