// Copyright (c) 2013 dacci.org

#include "net/async_server_socket.h"

#include <assert.h>

#include "net/async_socket.h"

AsyncServerSocket::AsyncServerSocket()
    : initialized_(), io_(), family_(AF_UNSPEC), protocol_() {
}

AsyncServerSocket::~AsyncServerSocket() {
  Close();
}

void AsyncServerSocket::Close() {
  initialized_ = false;
  family_ = AF_UNSPEC;
  protocol_ = 0;

  ServerSocket::Close();

#ifndef LEGACY_PLATFORM
  if (io_ != NULL) {
    ::WaitForThreadpoolIoCallbacks(io_, FALSE);
    ::CloseThreadpoolIo(io_);
    io_ = NULL;
  }
#endif  // LEGACY_PLATFORM
}

bool AsyncServerSocket::AcceptAsync(Listener* listener) {
  if (listener == NULL)
    return false;
  if (!IsValid() || !bound_)
    return false;

  AcceptContext* context = CreateAcceptContext();
  if (context == NULL)
    return false;

  context->listener = listener;

#ifdef LEGACY_PLATFORM
  BOOL succeeded = ::QueueUserWorkItem(AsyncWork, context,
                                       WT_EXECUTEINIOTHREAD);
#else   // LEGACY_PLATFORM
  BOOL succeeded = ::TrySubmitThreadpoolCallback(AsyncWork, context, NULL);
#endif  // LEGACY_PLATFORM
  if (!succeeded) {
    DestroyAcceptContext(context);
    return false;
  }

  return true;
}

OVERLAPPED* AsyncServerSocket::BeginAccept(HANDLE event) {
  if (event == NULL)
    return NULL;
  if (!IsValid() || !bound_)
    return NULL;

  AcceptContext* context = CreateAcceptContext();
  if (context == NULL)
    return NULL;

  context->event = event;
  ::ResetEvent(event);

#ifdef LEGACY_PLATFORM
  BOOL succeeded = ::QueueUserWorkItem(AsyncWork, context,
                                       WT_EXECUTEINIOTHREAD);
#else   // LEGACY_PLATFORM
  BOOL succeeded = ::TrySubmitThreadpoolCallback(AsyncWork, context, NULL);
#endif  // LEGACY_PLATFORM
  if (!succeeded) {
    DestroyAcceptContext(context);
    return NULL;
  }

  return context;
}

AsyncSocket* AsyncServerSocket::EndAccept(OVERLAPPED* overlapped) {
  AcceptContext* context = static_cast<AcceptContext*>(overlapped);
  if (context->server != this)
    return NULL;

  if (context->event != NULL)
    ::WaitForSingleObject(context->event, INFINITE);

  DWORD bytes = 0;
  BOOL succeeded = ::GetOverlappedResult(*context->client, context, &bytes,
                                         TRUE);
  AsyncSocket* client = NULL;
  if (succeeded && context->client->UpdateAcceptContext(descriptor_)) {
    client = context->client;
    context->client = NULL;
  }

  DestroyAcceptContext(context);

  return client;
}

AsyncServerSocket::AcceptContext* AsyncServerSocket::CreateAcceptContext() {
  if (!initialized_) {
    WSAPROTOCOL_INFO protocol_info;
    if (!GetOption(SOL_SOCKET, SO_PROTOCOL_INFO, &protocol_info))
      return NULL;

    family_ = protocol_info.iAddressFamily;
    protocol_ = protocol_info.iProtocol;

#ifdef LEGACY_PLATFORM
    if (!::BindIoCompletionCallback(*this, OnAccepted, 0))
#else   // LEGACY_PLATFORM
    io_ = ::CreateThreadpoolIo(*this, OnAccepted, this, NULL);
    if (io_ == NULL)
#endif  // LEGACY_PLATFORM
      return NULL;

    initialized_ = true;
  }

  AcceptContext* context = new AcceptContext();
  if (context == NULL)
    return NULL;

  ::memset(context, 0, sizeof(*context));

  context->client = new AsyncSocket();
  if (context->client == NULL) {
    DestroyAcceptContext(context);
    return NULL;
  }

  if (!context->client->Create(family_, SOCK_STREAM, protocol_)) {
    DestroyAcceptContext(context);
    return NULL;
  }

  context->buffer = new char[(address_length_ + 16) * 2];
  if (context->buffer == NULL) {
    DestroyAcceptContext(context);
    return NULL;
  }

  context->server = this;

  return context;
}

void AsyncServerSocket::DestroyAcceptContext(AcceptContext* context) {
  if (context->client != NULL)
    delete context->client;

  if (context->buffer != NULL)
    delete[] context->buffer;

  delete context;
}

#ifdef LEGACY_PLATFORM
DWORD CALLBACK AsyncServerSocket::AsyncWork(void* param) {
#else   // LEGACY_PLATFORM
void CALLBACK AsyncServerSocket::AsyncWork(PTP_CALLBACK_INSTANCE instance,
                                            void* param) {
#endif  // LEGACY_PLATFORM
  AcceptContext* context = static_cast<AcceptContext*>(param);

#ifndef LEGACY_PLATFORM
  ::StartThreadpoolIo(context->server->io_);
#endif  // LEGACY_PLATFORM

  DWORD bytes = 0;
  BOOL succeeded = ::AcceptEx(*context->server,
                              *context->client,
                              context->buffer,
                              0,
                              context->server->address_length_ + 16,
                              context->server->address_length_ + 16,
                              &bytes,
                              context);
  DWORD error = ::WSAGetLastError();
  if (!succeeded && error != ERROR_IO_PENDING) {
#ifdef LEGACY_PLATFORM
    OnAccepted(error, bytes, context);
#else   // LEGACY_PLATFORM
    ::CancelThreadpoolIo(context->server->io_);
    OnAccepted(instance, context->server, static_cast<OVERLAPPED*>(context),
               error, bytes, context->server->io_);
#endif  // LEGACY_PLATFORM
  }

#ifdef LEGACY_PLATFORM
  return 0;
#endif  // LEGACY_PLATFORM
}

#ifdef LEGACY_PLATFORM
void CALLBACK AsyncServerSocket::OnAccepted(DWORD error, DWORD bytes,
                                            OVERLAPPED* overlapped) {
  AcceptContext* context = static_cast<AcceptContext*>(overlapped);
#else   // LEGACY_PLATFORM
void CALLBACK AsyncServerSocket::OnAccepted(PTP_CALLBACK_INSTANCE instance,
                                            void* self,
                                            void* overlapped,
                                            ULONG error,
                                            ULONG_PTR bytes,
                                            PTP_IO io) {
  AcceptContext* context =
      static_cast<AcceptContext*>(static_cast<OVERLAPPED*>(overlapped));
#endif  // LEGACY_PLATFORM

  if (context->listener != NULL) {
    AsyncServerSocket* server = context->server;
    Listener* listener = context->listener;
    AsyncSocket* client = server->EndAccept(context);
    listener->OnAccepted(server, client, error);
  } else if (context->event != NULL) {
    ::SetEvent(context->event);
  } else {
    assert(false);
  }
}
