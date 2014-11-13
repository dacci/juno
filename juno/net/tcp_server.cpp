// Copyright (c) 2013 dacci.org

#include "net/tcp_server.h"

#include <madoka/concurrent/lock_guard.h>

#include "net/socket_channel.h"
#include "service/service.h"

using ::madoka::net::AsyncServerSocket;
using ::madoka::net::AsyncSocket;

TcpServer::TcpServer() : service_() {
}

TcpServer::~TcpServer() {
  Stop();
}

bool TcpServer::Setup(const char* address, int port) {
  if (port <= 0 || 65536 <= port)
    return false;

  if (address[0] == '*')
    address = nullptr;

  resolver_.SetFlags(AI_PASSIVE);
  resolver_.SetType(SOCK_STREAM);
  if (!resolver_.Resolve(address, port))
    return false;

  bool succeeded = true;

  for (const auto& end_point : resolver_) {
    auto server = std::unique_ptr<AsyncServerSocket>(new AsyncServerSocket());
    if (server == nullptr)
      break;

    if (server->Bind(end_point) && server->Listen(SOMAXCONN))
      servers_.push_back(std::move(server));
    else
      succeeded = false;
  }

  return succeeded;
}

bool TcpServer::Start() {
  if (service_ == nullptr || servers_.empty())
    return false;

  madoka::concurrent::LockGuard guard(&lock_);

  for (auto& server : servers_)
    server->AcceptAsync(this);

  return true;
}

void TcpServer::Stop() {
  madoka::concurrent::LockGuard guard(&lock_);

  for (auto& server : servers_)
    server->Close();

  while (!servers_.empty())
    empty_.Sleep(&lock_);
}

void TcpServer::OnAccepted(AsyncServerSocket* server, AsyncSocket* client,
                           DWORD error) {
  if (error == 0) {
    server->AcceptAsync(this);

    auto peer = std::make_shared<SocketChannel>(
        SocketChannel::AsyncSocketPtr(client));
    if (service_->OnAccepted(peer))
      peer.reset();
  } else {
    service_->OnError(error);
    DeleteServer(server);
  }
}

void TcpServer::DeleteServer(AsyncServerSocket* server) {
  ServerSocketPair* pair = new ServerSocketPair(this, server);
  if (pair != nullptr &&
      TrySubmitThreadpoolCallback(DeleteServerImpl, pair, nullptr))
    return;

  delete pair;
  DeleteServerImpl(server);
}

void CALLBACK TcpServer::DeleteServerImpl(PTP_CALLBACK_INSTANCE instance,
                                          void* param) {
  ServerSocketPair* pair = static_cast<ServerSocketPair*>(param);
  pair->first->DeleteServerImpl(pair->second);
  delete pair;
}

void TcpServer::DeleteServerImpl(AsyncServerSocket* server) {
  madoka::concurrent::LockGuard guard(&lock_);

  server->Close();

  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i) {
    if (i->get() == server) {
      servers_.erase(i);
      break;
    }
  }

  if (servers_.empty())
    empty_.WakeAll();
}
