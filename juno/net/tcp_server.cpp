// Copyright (c) 2013 dacci.org

#include "net/tcp_server.h"
#include "net/async_server_socket.h"
#include "net/service_provider.h"

TcpServer::TcpServer() : service_() {
}

TcpServer::~TcpServer() {
  Stop();

  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i)
    delete *i;
  servers_.clear();
}

bool TcpServer::Setup(const char* address, int port) {
  if (port <= 0 || 65536 <= port)
    return false;

  if (address[0] == '*')
    address = NULL;

  char service[8];
  ::sprintf_s(service, "%d", port);

  resolver_.ai_flags = AI_PASSIVE;
  resolver_.ai_socktype = SOCK_STREAM;
  if (!resolver_.Resolve(address, service))
    return false;

  bool succeeded = false;

  for (auto i = resolver_.begin(), l = resolver_.end(); i != l; ++i) {
    AsyncServerSocket* server = new AsyncServerSocket();
    if (server == NULL)
      break;

    if (server->Bind(*i) && server->Listen(SOMAXCONN)) {
      succeeded = true;
      servers_.push_back(server);
    } else {
      delete server;
    }
  }

  return succeeded;
}

bool TcpServer::Start() {
  if (service_ == NULL || servers_.empty())
    return false;

  bool succeeded = false;

  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i) {
    if ((*i)->AcceptAsync(service_))
      succeeded = true;
  }

  return succeeded;
}

void TcpServer::Stop() {
  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i)
    (*i)->Close();
}
