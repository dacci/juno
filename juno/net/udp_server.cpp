// Copyright (c) 2013 dacci.org

#include "net/udp_server.h"

#include <memory>

#include "net/datagram.h"
#include "service/service.h"

using ::madoka::net::AsyncSocket;

UdpServer::UdpServer() : service_(), empty_(&lock_) {
}

UdpServer::~UdpServer() {
  Stop();
}

bool UdpServer::Setup(const char* address, int port) {
  if (port <= 0 || 65536 <= port)
    return false;

  if (address[0] == '*')
    address = nullptr;

  resolver_.SetFlags(AI_PASSIVE);
  resolver_.SetType(SOCK_DGRAM);
  if (!resolver_.Resolve(address, port))
    return false;

  bool succeeded = false;

  for (const auto& end_point : resolver_) {
    auto buffer = std::make_unique<char[]>(kBufferSize);
    if (buffer == nullptr)
      break;

    auto server = std::make_unique<AsyncSocket>();
    if (server == nullptr)
      break;

    if (server->Bind(end_point)) {
      succeeded = true;
      buffers_.insert({ server.get(), std::move(buffer) });
      servers_.push_back(std::move(server));
    }
  }

  return succeeded;
}

bool UdpServer::Start() {
  if (service_ == nullptr || servers_.empty())
    return false;

  base::AutoLock guard(lock_);

  for (auto& pair : buffers_)
    pair.first->ReceiveFromAsync(pair.second.get(), kBufferSize, 0, this);

  return true;
}

void UdpServer::Stop() {
  base::AutoLock guard(lock_);

  for (auto& server : servers_)
    server->Close();

  while (!servers_.empty())
    empty_.Wait();
}

void UdpServer::OnReceived(AsyncSocket* socket, HRESULT result,
                           void* buffer, int length, int flags) {
  delete socket;
}

void UdpServer::OnReceivedFrom(AsyncSocket* socket, HRESULT result,
                               void* buffer, int length, int flags,
                               const sockaddr* address, int address_length) {
  if (SUCCEEDED(result)) {
    do {
      auto datagram = std::make_shared<Datagram>();
      if (datagram == nullptr) {
        result = E_OUTOFMEMORY;
        break;
      }

      datagram->data.reset(new char[length]);
      if (datagram->data == nullptr) {
        result = E_OUTOFMEMORY;
        break;
      }

      for (auto& server : servers_) {
        if (server.get() == socket) {
          datagram->socket = server;
          break;
        }
      }

      datagram->data_length = length;
      memmove(datagram->data.get(), buffer, length);
      datagram->from_length = address_length;
      memmove(&datagram->from, address, address_length);

      socket->ReceiveFromAsync(buffer, kBufferSize, 0, this);

      service_->OnReceivedFrom(datagram);
    } while (false);
  }

  if (FAILED(result)) {
    service_->OnError(HRESULT_CODE(result));
    DeleteServer(socket);
  }
}

void UdpServer::DeleteServer(AsyncSocket* server) {
  ServerSocketPair* pair = new ServerSocketPair(this, server);
  if (pair == nullptr ||
      !TrySubmitThreadpoolCallback(DeleteServerImpl, pair, nullptr)) {
    delete pair;
    DeleteServerImpl(server);
  }
}

void CALLBACK UdpServer::DeleteServerImpl(PTP_CALLBACK_INSTANCE instance,
                                          void* context) {
  ServerSocketPair* pair = static_cast<ServerSocketPair*>(context);
  pair->first->DeleteServerImpl(pair->second);
  delete pair;
}

void UdpServer::DeleteServerImpl(AsyncSocket* server) {
  std::unique_ptr<char[]> removed_buffer;
  std::shared_ptr<AsyncSocket> removed_server;

  lock_.Acquire();

  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i) {
    if (i->get() == server) {
      removed_buffer = std::move(buffers_.at(server));
      buffers_.erase(server);

      removed_server = std::move(*i);
      servers_.erase(i);

      if (servers_.empty())
        empty_.Broadcast();

      break;
    }
  }

  lock_.Release();

  if (removed_server != nullptr)
    removed_server->Close();
}
