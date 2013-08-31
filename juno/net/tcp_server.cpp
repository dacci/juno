// Copyright (c) 2013 dacci.org

#include "net/tcp_server.h"

#include <madoka/concurrent/lock_guard.h>

#include "net/async_server_socket.h"
#include "net/async_socket.h"
#include "net/service_provider.h"

TcpServer::TcpServer() : service_(), count_() {
  event_ = ::CreateEvent(NULL, TRUE, TRUE, NULL);
}

TcpServer::~TcpServer() {
  Stop();

  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i)
    delete *i;
  servers_.clear();

  ::CloseHandle(event_);
}

bool TcpServer::Setup(const char* address, int port) {
  if (port <= 0 || 65536 <= port)
    return false;

  if (address[0] == '*')
    address = NULL;

  resolver_.ai_flags = AI_PASSIVE;
  resolver_.ai_socktype = SOCK_STREAM;
  if (!resolver_.Resolve(address, port))
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

  madoka::concurrent::LockGuard lock(&critical_section_);

  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i) {
    if ((*i)->AcceptAsync(this)) {
      succeeded = true;
      if (count_++ == 0)
        ::ResetEvent(event_);
    } else {
      (*i)->Close();
    }
  }

  return succeeded;
}

void TcpServer::Stop() {
  madoka::concurrent::LockGuard lock(&critical_section_);

  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i)
    (*i)->Close();

  while (true) {
    critical_section_.Unlock();
    ::WaitForSingleObject(event_, INFINITE);
    critical_section_.Lock();

    if (count_ == 0)
      break;
  }
}

void TcpServer::OnAccepted(AsyncServerSocket* server, AsyncSocket* client,
                           DWORD error) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  if (error == 0) {
    if (service_->OnAccepted(client))
      client = NULL;

    if (server->AcceptAsync(this))
      return;
  } else {
    service_->OnError(error);
  }

  if (client != NULL)
    delete client;

  server->Close();

  if (--count_ == 0)
    ::SetEvent(event_);
}
