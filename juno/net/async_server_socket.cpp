// Copyright (c) 2013 dacci.org

#include "net/async_server_socket.h"
#include "net/async_socket.h"

AsyncServerSocket::AsyncServerSocket() : family_(), protocol_() {
}

AsyncServerSocket::~AsyncServerSocket() {
}

bool AsyncServerSocket::Bind(const addrinfo* end_point) {
  if (!ServerSocket::Bind(end_point))
    return false;

  family_ = end_point->ai_family;
  protocol_ = end_point->ai_protocol;

  return true;
}

AsyncServerSocket::AcceptContext* AsyncServerSocket::BeginAsyncAccept(
    HANDLE event) {
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

AsyncSocket* AsyncServerSocket::EndAsyncAccept(AcceptContext* context) {
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
