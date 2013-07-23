// Copyright (c) 2013 dacci.org

#include "net/async_server_socket.h"
#include "net/async_socket.h"

AsyncServerSocket::AsyncServerSocket() : family_(), protocol_() {
}

AsyncServerSocket::~AsyncServerSocket() {
  Close();
}

void AsyncServerSocket::Close() {
  ServerSocket::Close();
}

bool AsyncServerSocket::Bind(const addrinfo* end_point) {
  if (!ServerSocket::Bind(end_point))
    return false;

  family_ = end_point->ai_family;
  protocol_ = end_point->ai_protocol;

  ::BindIoCompletionCallback(*this, OnAccepted, 0);

  return true;
}

bool AsyncServerSocket::AcceptAsync(Listener* listener) {
  if (!IsValid() || !bound_)
    return false;

  AcceptContext* context = CreateAcceptContext();
  if (context == NULL)
    return false;

  context->listener = listener;

  BOOL succeeded = ::QueueUserWorkItem(AcceptWork, context,
                                       WT_EXECUTEINIOTHREAD);
  if (!succeeded) {
    DestroyAcceptContext(context);
    return false;
  }

  return true;
}

OVERLAPPED* AsyncServerSocket::BeginAccept(HANDLE event) {
  if (!IsValid() || !bound_)
    return NULL;

  AcceptContext* context = CreateAcceptContext();
  if (context == NULL)
    return NULL;

  context->hEvent = event;

  BOOL succeeded = ::QueueUserWorkItem(AcceptWork, context,
                                       WT_EXECUTEINIOTHREAD);
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

  AsyncSocket* client = context->client;

  DWORD bytes = 0;
  BOOL succeeded = ::GetOverlappedResult(*client, context, &bytes, TRUE);
  if (succeeded) {
    context->client = NULL;
    client->SetOption(SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, descriptor_);
    client->connected_ = true;
  } else {
    client = NULL;
  }

  DestroyAcceptContext(context);

  return client;
}

AsyncServerSocket::AcceptContext* AsyncServerSocket::CreateAcceptContext() {
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

DWORD CALLBACK AsyncServerSocket::AcceptWork(void* param) {
  AcceptContext* context = static_cast<AcceptContext*>(param);

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
  if (!succeeded && error != ERROR_IO_PENDING)
    OnAccepted(error, bytes, context);

  return 0;
}

void CALLBACK AsyncServerSocket::OnAccepted(DWORD error, DWORD bytes,
                                            OVERLAPPED* overlapped) {
  AcceptContext* context = static_cast<AcceptContext*>(overlapped);

  if (context->hEvent == NULL) {
    AsyncServerSocket* server = context->server;
    Listener* listener = context->listener;
    AsyncSocket* client = server->EndAccept(overlapped);
    listener->OnAccepted(client, error);
  }
}
