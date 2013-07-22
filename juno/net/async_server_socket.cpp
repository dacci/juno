// Copyright (c) 2013 dacci.org

#include "net/async_server_socket.h"
#include "net/async_socket.h"

AsyncServerSocket::AsyncServerSocket() : io_(), family_(), protocol_() {
}

AsyncServerSocket::~AsyncServerSocket() {
  Close();
}

void AsyncServerSocket::Close() {
  ServerSocket::Close();

  if (io_ != NULL) {
    ::WaitForThreadpoolIoCallbacks(io_, FALSE);
    ::CloseThreadpoolIo(io_);
    io_ = NULL;
  }
}

bool AsyncServerSocket::SetThreadpool(PTP_CALLBACK_ENVIRON environment) {
  if (!IsValid())
    return false;

  PTP_IO new_io = ::CreateThreadpoolIo(*this, OnAccepted, this, environment);
  if (new_io == NULL)
    return false;

  if (io_ != NULL) {
    ::CloseThreadpoolIo(io_);
    io_ = NULL;
  }

  io_ = new_io;

  return true;
}

bool AsyncServerSocket::Bind(const addrinfo* end_point) {
  if (!ServerSocket::Bind(end_point))
    return false;

  family_ = end_point->ai_family;
  protocol_ = end_point->ai_protocol;

  return true;
}

AsyncServerSocket::AcceptContext* AsyncServerSocket::BeginAccept(HANDLE event) {
  if (!IsValid() || !bound_)
    return NULL;

  AsyncSocket* peer = new AsyncSocket();
  if (!peer->Create(family_, SOCK_STREAM, protocol_)) {
    delete peer;
    return NULL;
  }

  AcceptContext* context = new AcceptContext();
  context->hEvent = event;
  context->peer = peer;
  context->buffer = new char[(address_length_ + 16) * 2];

  BOOL succeeded = ::AcceptEx(descriptor_,
                              *peer,
                              context->buffer,
                              0,
                              address_length_ + 16,
                              address_length_ + 16,
                              &context->bytes,
                              context);
  DWORD error = ::WSAGetLastError();
  if (!succeeded && error != ERROR_IO_PENDING) {
    delete peer;
    delete[] context->buffer;
    delete context;
    return NULL;
  }

  return context;
}

AsyncServerSocket::AcceptContext* AsyncServerSocket::BeginAccept() {
  if (!IsValid() || !bound_ || io_ == NULL)
    return NULL;

  ::StartThreadpoolIo(io_);

  AcceptContext* context = BeginAccept(NULL);
  if (context == NULL)
    ::CancelThreadpoolIo(io_);

  return context;
}

AsyncSocket* AsyncServerSocket::EndAccept(AcceptContext* context) {
  AsyncSocket* peer = context->peer;
  BOOL succeeded = ::GetOverlappedResult(*peer, context,
                                         &context->bytes, TRUE);

  delete[] context->buffer;
  delete context;

  if (succeeded) {
    peer->SetOption(SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, descriptor_);
    peer->connected_ = true;
  } else {
    delete peer;
    peer = NULL;
  }

  return peer;
}

void CALLBACK AsyncServerSocket::OnAccepted(PTP_CALLBACK_INSTANCE instance,
                                            PVOID context, PVOID overlapped,
                                            ULONG io_result, ULONG_PTR bytes,
                                            PTP_IO io) {
  AsyncServerSocket* server = static_cast<AsyncServerSocket*>(context);
  AcceptContext* accept_context = static_cast<AcceptContext*>(
      static_cast<OVERLAPPED*>(overlapped));

  AsyncSocket* peer = server->EndAccept(accept_context);
  server->OnAccepted(peer, io_result);
}

void AsyncServerSocket::OnAccepted(AsyncSocket* peer, ULONG error) {
  if (error != 0)
    return;

  ::fprintf(stderr, "[%u] %p, %lu\n", __LINE__, peer, error);
  peer->Shutdown(SD_BOTH);
  delete peer;
}
