// Copyright (c) 2013 dacci.org

#include "service/tcp_server.h"

#include "io/net/socket_channel.h"
#include "service/service.h"

TcpServer::TcpServer()
    : channel_customizer_(nullptr), service_(nullptr), empty_(&lock_) {}

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
  auto result = resolver_.Resolve(address, port);
  if (FAILED(result))
    return false;

  auto succeeded = false;

  for (auto& end_point : resolver_) {
    auto server = std::make_unique<AsyncServerSocket>();
    if (server == nullptr)
      break;

    if (server->Bind(end_point.get()) && server->Listen(SOMAXCONN)) {
      servers_.push_back(std::move(server));
      succeeded = true;
    }
  }

  return succeeded;
}

bool TcpServer::Start() {
  if (service_ == nullptr || servers_.empty())
    return false;

  base::AutoLock guard(lock_);

  for (auto& server : servers_)
    server->AcceptAsync(this);

  return true;
}

void TcpServer::Stop() {
  base::AutoLock guard(lock_);

  for (auto& server : servers_)
    server->Close();

  while (!servers_.empty())
    empty_.Wait();
}

void TcpServer::DeleteServer(AsyncServerSocket* server) {
  auto pair = new ServerSocketPair(this, server);
  if (pair == nullptr ||
      !TrySubmitThreadpoolCallback(DeleteServerImpl, pair, nullptr)) {
    delete pair;
    DeleteServerImpl(server);
  }
}

void CALLBACK TcpServer::DeleteServerImpl(PTP_CALLBACK_INSTANCE /*instance*/,
                                          void* param) {
  auto pair = static_cast<ServerSocketPair*>(param);
  pair->first->DeleteServerImpl(pair->second);
  delete pair;
}

void TcpServer::DeleteServerImpl(AsyncServerSocket* server) {
  std::unique_ptr<AsyncServerSocket> removed;
  base::AutoLock guard(lock_);

  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i) {
    if (i->get() == server) {
      removed = std::move(*i);
      servers_.erase(i);

      if (servers_.empty())
        empty_.Broadcast();

      break;
    }
  }
}

void TcpServer::OnAccepted(AsyncServerSocket* server, HRESULT result,
                           AsyncServerSocket::Context* context) {
  do {
    if (FAILED(result))
      break;

    std::shared_ptr<Channel> channel =
        server->EndAccept<SocketChannel>(context, &result);
    if (channel == nullptr)
      break;

    {
      base::AutoLock guard(lock_);

      if (channel_customizer_ != nullptr)
        channel = channel_customizer_->Customize(channel);
    }

    service_->OnAccepted(channel);

    result = server->AcceptAsync(this);
  } while (false);

  if (FAILED(result)) {
    service_->OnError(HRESULT_CODE(result));
    DeleteServer(server);
  }
}
