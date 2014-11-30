// Copyright (c) 2013 dacci.org

#include "net/udp_server.h"

#include <assert.h>

#include <madoka/concurrent/lock_guard.h>

#include <memory>

#include "service/service.h"

using ::madoka::net::AsyncDatagramSocket;

UdpServer::UdpServer() : service_() {
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

    auto server = std::make_unique<AsyncDatagramSocket>();
    if (server == nullptr)
      break;

    if (server->Bind(end_point)) {
      succeeded = true;
      buffers_.insert(std::make_pair(server.get(), std::move(buffer)));
      servers_.push_back(std::move(server));
    }
  }

  return succeeded;
}

bool UdpServer::Start() {
  if (service_ == nullptr || servers_.empty())
    return false;

  madoka::concurrent::LockGuard guard(&lock_);

  for (auto& server : servers_)
    server->ReceiveFromAsync(buffers_.at(server.get()).get(), kBufferSize, 0,
                             this);

  return true;
}

void UdpServer::Stop() {
  madoka::concurrent::LockGuard guard(&lock_);

  for (auto& server : servers_)
    server->Close();

  while (!servers_.empty())
    empty_.Sleep(&lock_);
}

void UdpServer::OnReceived(madoka::net::AsyncSocket* socket, DWORD error,
                           void* buffer, int length) {
  delete socket;
}

void UdpServer::OnReceivedFrom(AsyncDatagramSocket* socket, DWORD error,
                               void* buffer, int length, sockaddr* from,
                               int from_length) {
  if (error == 0) {
    do {
      auto received = std::make_unique<char[]>(
          sizeof(Service::Datagram) + length + from_length);
      if (received == nullptr) {
        error = E_OUTOFMEMORY;
        break;
      }

      const auto& buffer = buffers_.at(socket);
      if (buffer == nullptr) {
        error = E_ABORT;
        break;
      }

      Service::Datagram* datagram =
          reinterpret_cast<Service::Datagram*>(received.get());
      datagram->service = service_;
      datagram->socket = socket;

      datagram->data_length = length;
      datagram->data = received.get() + sizeof(Service::Datagram);
      ::memmove(datagram->data, buffer.get(), length);

      datagram->from_length = from_length;
      datagram->from = reinterpret_cast<sockaddr*>(
          received.get() + sizeof(Service::Datagram) + length);
      ::memmove(datagram->from, from, from_length);

      BOOL succeeded = ::TrySubmitThreadpoolCallback(Dispatch, datagram,
                                                     nullptr);
      if (!succeeded) {
        error = ::GetLastError();
        break;
      }

      socket->ReceiveFromAsync(buffer.get(), kBufferSize, 0, this);
    } while (false);
  }

  if (error != 0) {
    service_->OnError(error);
    DeleteServer(socket);
  }
}

void CALLBACK UdpServer::Dispatch(PTP_CALLBACK_INSTANCE instance,
                                  void* context) {
  Service::Datagram* datagram = static_cast<Service::Datagram*>(context);

  bool succeeded = datagram->service->OnReceivedFrom(datagram);
  if (!succeeded)
    delete[] static_cast<char*>(context);
}

void UdpServer::DeleteServer(AsyncDatagramSocket* server) {
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

void UdpServer::DeleteServerImpl(AsyncDatagramSocket* server) {
  std::unique_ptr<char[]> removed_buffer;
  std::unique_ptr<AsyncDatagramSocket> removed_server;
  madoka::concurrent::LockGuard guard(&lock_);

  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i) {
    if (i->get() == server) {
      removed_buffer = std::move(buffers_.at(server));
      buffers_.erase(server);

      removed_server = std::move(*i);
      servers_.erase(i);

      if (servers_.empty())
        empty_.WakeAll();

      break;
    }
  }
}
