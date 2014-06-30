// Copyright (c) 2013 dacci.org

#include "net/udp_server.h"

#include <assert.h>

#include <memory>

#include "app/service.h"

using ::madoka::net::AsyncDatagramSocket;

UdpServer::UdpServer() : service_(), count_() {
  event_ = ::CreateEvent(nullptr, TRUE, TRUE, nullptr);
}

UdpServer::~UdpServer() {
  Stop();

  for (auto& server : servers_)
    delete server;
  servers_.clear();

  for (auto& pair : buffers_)
    delete[] pair.second;
  buffers_.clear();

  ::CloseHandle(event_);
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
    std::unique_ptr<char[]> buffer(new char[kBufferSize]);
    if (buffer == nullptr)
      break;

    AsyncDatagramSocket* server = new AsyncDatagramSocket();
    if (server == nullptr)
      break;

    if (server->Bind(end_point)) {
      succeeded = true;
      servers_.push_back(server);
      buffers_.insert(std::make_pair(server, buffer.release()));
    } else {
      delete server;
    }
  }

  return succeeded;
}

bool UdpServer::Start() {
  if (service_ == nullptr || servers_.empty())
    return false;

  bool succeeded = false;

  for (auto& socket : servers_) {
    if (socket->ReceiveFromAsync(buffers_[socket], kBufferSize, 0, this)) {
      succeeded = true;
      if (count_++ == 0)
        ::ResetEvent(event_);
    } else {
      socket->Close();
    }
  }

  return succeeded;
}

void UdpServer::Stop() {
  for (auto& server : servers_)
    server->Close();

  ::WaitForSingleObject(event_, INFINITE);
}

void UdpServer::OnReceivedFrom(AsyncDatagramSocket* socket, DWORD error,
                               void* buffer, int length, sockaddr* from,
                               int from_length) {
  if (error == 0) {
    char* received = nullptr;

    do {
      received = new char[sizeof(Service::Datagram) + length + from_length];
      if (received == nullptr)
        break;

      char* buffer = buffers_[socket];

      Service::Datagram* datagram =
          reinterpret_cast<Service::Datagram*>(received);
      datagram->service = service_;
      datagram->socket = socket;

      datagram->data_length = length;
      datagram->data = received + sizeof(Service::Datagram);
      ::memmove(datagram->data, buffer, length);

      datagram->from_length = from_length;
      datagram->from = reinterpret_cast<sockaddr*>(
          received + sizeof(Service::Datagram) + length);
      ::memmove(datagram->from, from, from_length);

      BOOL succeeded = ::TrySubmitThreadpoolCallback(Dispatch, datagram,
                                                     nullptr);
      if (!succeeded) {
        error = ::GetLastError();
        break;
      }

      if (!socket->ReceiveFromAsync(buffer, kBufferSize, 0, this))
        break;

      return;
    } while (false);

    if (received != nullptr)
      delete[] received;
  }

  service_->OnError(error);

  if (::InterlockedDecrement(&count_) == 0)
    ::SetEvent(event_);
}

void CALLBACK UdpServer::Dispatch(PTP_CALLBACK_INSTANCE instance,
                                  void* context) {
  Service::Datagram* datagram = static_cast<Service::Datagram*>(context);

  bool succeeded = datagram->service->OnReceivedFrom(datagram);
  if (!succeeded)
    delete[] static_cast<char*>(context);
}
