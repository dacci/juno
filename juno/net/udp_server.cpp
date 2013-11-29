// Copyright (c) 2013 dacci.org

#include "net/udp_server.h"

#include <assert.h>

#include <memory>

#include "net/service_provider.h"

UdpServer::UdpServer() : service_(), count_() {
  event_ = ::CreateEvent(NULL, TRUE, TRUE, NULL);
}

UdpServer::~UdpServer() {
  Stop();

  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i)
    delete *i;
  servers_.clear();

  for (auto i = buffers_.begin(), l = buffers_.end(); i != l; ++i)
    delete[] i->second;
  buffers_.clear();

  ::CloseHandle(event_);
}

bool UdpServer::Setup(const char* address, int port) {
  if (port <= 0 || 65536 <= port)
    return false;

  if (address[0] == '*')
    address = NULL;

  resolver_.ai_flags = AI_PASSIVE;
  resolver_.ai_socktype = SOCK_DGRAM;
  if (!resolver_.Resolve(address, port))
    return false;

  bool succeeded = false;

  for (auto i = resolver_.begin(), l = resolver_.end(); i != l; ++i) {
    std::unique_ptr<char[]> buffer(new char[kBufferSize]);
    if (buffer == NULL)
      break;

    AsyncDatagramSocket* server = new AsyncDatagramSocket();
    if (server == NULL)
      break;

    if (server->Bind(*i)) {
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
  if (service_ == NULL || servers_.empty())
    return false;

  bool succeeded = false;

  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i) {
    AsyncDatagramSocket* socket = *i;

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
  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i)
    (*i)->Close();

  ::WaitForSingleObject(event_, INFINITE);
}

void UdpServer::OnReceived(AsyncDatagramSocket* socket, DWORD error,
                           int length) {
  assert(false);
}

void UdpServer::OnReceivedFrom(AsyncDatagramSocket* socket, DWORD error,
                               int length, sockaddr* from, int from_length) {
  if (error == 0) {
    char* received = NULL;

    do {
      received = new char[sizeof(ServiceProvider::Datagram) + length +
                          from_length];
      if (received == NULL)
        break;

      char* buffer = buffers_[socket];

      ServiceProvider::Datagram* datagram =
          reinterpret_cast<ServiceProvider::Datagram*>(received);
      datagram->service = service_;
      datagram->socket = socket;

      datagram->data_length = length;
      datagram->data = received + sizeof(ServiceProvider::Datagram);
      ::memmove(datagram->data, buffer, length);

      datagram->from_length = from_length;
      datagram->from = reinterpret_cast<sockaddr*>(
          received + sizeof(ServiceProvider::Datagram) + length);
      ::memmove(datagram->from, from, from_length);

      BOOL succeeded = FALSE;
#ifdef LEGACY_PLATFORM
      succeeded = ::QueueUserWorkItem(Dispatch, datagram, 0);
#else   // LEGACY_PLATFORM
      succeeded = ::TrySubmitThreadpoolCallback(Dispatch, datagram, NULL);
#endif  // LEGACY_PLATFORM
      if (!succeeded) {
        error = ::GetLastError();
        break;
      }

      if (!socket->ReceiveFromAsync(buffer, kBufferSize, 0, this))
        break;

      return;
    } while (false);

    if (received != NULL)
      delete[] received;
  }

  service_->OnError(error);

  if (::InterlockedDecrement(&count_) == 0)
    ::SetEvent(event_);
}

void UdpServer::OnSent(AsyncDatagramSocket* socket, DWORD error, int length) {
  assert(false);
}

void UdpServer::OnSentTo(AsyncDatagramSocket* socket, DWORD error, int length,
                         sockaddr* to, int to_length) {
  assert(false);
}

#ifdef LEGACY_PLATFORM
DWORD CALLBACK UdpServer::Dispatch(void* context) {
#else   // LEGACY_PLATFORM
void CALLBACK UdpServer::Dispatch(PTP_CALLBACK_INSTANCE instance,
                                  void* context) {
#endif  // LEGACY_PLATFORM
  ServiceProvider::Datagram* datagram =
      static_cast<ServiceProvider::Datagram*>(context);

  bool succeeded = datagram->service->OnReceivedFrom(datagram);
  if (!succeeded)
    delete[] static_cast<char*>(context);

#ifdef LEGACY_PLATFORM
  return 0;
#endif  // LEGACY_PLATFORM
}
