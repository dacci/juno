// Copyright (c) 2013 dacci.org

#include "net/async_datagram_socket.h"

#include <assert.h>

bool AsyncDatagramSocket::Bind(const addrinfo* end_point) {
  if (!DatagramSocket::Bind(end_point))
    return false;

  if (!Init()) {
    Close();
    return false;
  }

  return true;
}

bool AsyncDatagramSocket::Connect(const addrinfo* end_point) {
  if (!DatagramSocket::Connect(end_point))
    return false;

  if (!Init()) {
    Close();
    return false;
  }

  return true;
}

bool AsyncDatagramSocket::ReceiveAsync(void* buffer, int size, int flags,
                                       Listener* listener) {
  if (listener == NULL)
    return false;
  if (!IsValid() || !connected_)
    return false;

  AsyncContext* context = CreateAsyncContext();
  if (context == NULL)
    return false;

  context->len = size;
  context->buf = static_cast<char*>(buffer);
  context->action = Receiving;
  context->flags = flags;
  context->listener = listener;

  BOOL succeeded = ::QueueUserWorkItem(AsyncWork, context,
                                       WT_EXECUTEINIOTHREAD);
  if (!succeeded) {
    DestroyAsyncContext(context);
    return false;
  }

  return true;
}

OVERLAPPED* AsyncDatagramSocket::BeginReceive(void* buffer, int size, int flags,
                                              HANDLE event) {
  if (event == NULL)
    return NULL;
  if (!IsValid() || !connected_)
    return NULL;

  AsyncContext* context = CreateAsyncContext();
  if (context == NULL)
    return NULL;

  context->len = size;
  context->buf = static_cast<char*>(buffer);
  context->action = Receiving;
  context->flags = flags;
  context->event = event;
  ::ResetEvent(event);

  BOOL succeeded = ::QueueUserWorkItem(AsyncWork, context,
                                       WT_EXECUTEINIOTHREAD);
  if (!succeeded) {
    DestroyAsyncContext(context);
    return NULL;
  }

  return context;
}

int AsyncDatagramSocket::EndReceive(OVERLAPPED* overlapped) {
  AsyncContext* context = static_cast<AsyncContext*>(overlapped);
  if (context->action != Receiving || context->socket != this)
    return SOCKET_ERROR;

  if (context->event != NULL)
    ::WaitForSingleObject(context->event, INFINITE);

  DWORD bytes = 0;
  BOOL succeeded = ::GetOverlappedResult(*context->socket, context, &bytes,
                                         FALSE);
  DestroyAsyncContext(context);

  if (succeeded)
    return bytes;
  else
    return SOCKET_ERROR;
}

bool AsyncDatagramSocket::SendAsync(const void* buffer, int size, int flags,
                                    Listener* listener) {
  if (listener == NULL)
    return false;
  if (!IsValid() || !connected_)
    return false;

  AsyncContext* context = CreateAsyncContext();
  if (context == NULL)
    return false;

  context->len = size;
  context->buf = static_cast<char*>(const_cast<void*>(buffer));
  context->action = Sending;
  context->flags = flags;
  context->listener = listener;

  BOOL succeeded = ::QueueUserWorkItem(AsyncWork, context,
                                       WT_EXECUTEINIOTHREAD);
  if (!succeeded) {
    DestroyAsyncContext(context);
    return false;
  }

  return true;
}

OVERLAPPED* AsyncDatagramSocket::BeginSend(const void* buffer, int size,
                                           int flags, HANDLE event) {
  if (event == NULL)
    return NULL;
  if (!IsValid() || !connected_)
    return NULL;

  AsyncContext* context = CreateAsyncContext();
  if (context == NULL)
    return NULL;

  context->len = size;
  context->buf = static_cast<char*>(const_cast<void*>(buffer));
  context->action = Sending;
  context->flags = flags;
  context->event = event;
  ::ResetEvent(event);

  BOOL succeeded = ::QueueUserWorkItem(AsyncWork, context,
                                       WT_EXECUTEINIOTHREAD);
  if (!succeeded) {
    DestroyAsyncContext(context);
    return NULL;
  }

  return context;
}

int AsyncDatagramSocket::EndSend(OVERLAPPED* overlapped) {
  AsyncContext* context = static_cast<AsyncContext*>(overlapped);
  if (context->action != Sending || context->socket != this)
    return SOCKET_ERROR;

  if (context->event != NULL)
    ::WaitForSingleObject(context->event, INFINITE);

  DWORD bytes = 0;
  BOOL succeeded = ::GetOverlappedResult(*context->socket, context, &bytes,
                                         FALSE);
  DestroyAsyncContext(context);

  if (succeeded)
    return bytes;
  else
    return SOCKET_ERROR;
}

bool AsyncDatagramSocket::ReceiveFromAsync(void* buffer, int size, int flags,
                                           Listener* listener) {
  if (listener == NULL)
    return false;
  if (!IsValid() || !bound_)
    return false;

  AsyncContext* context = CreateAsyncContext();
  if (context == NULL)
    return false;

  context->len = size;
  context->buf = static_cast<char*>(buffer);
  context->action = ReceivingFrom;
  context->flags = flags;
  context->address_length = 64;
  context->address = static_cast<sockaddr*>(::malloc(context->address_length));
  context->listener = listener;

  BOOL succeeded = ::QueueUserWorkItem(AsyncWork, context,
                                       WT_EXECUTEINIOTHREAD);
  if (!succeeded) {
    DestroyAsyncContext(context);
    return false;
  }

  return true;
}

OVERLAPPED* AsyncDatagramSocket::BeginReceiveFrom(void* buffer, int size,
                                                  int flags, HANDLE event) {
  if (event == NULL)
    return NULL;
  if (!IsValid() || !bound_)
    return NULL;

  AsyncContext* context = CreateAsyncContext();
  if (context == NULL)
    return NULL;

  context->len = size;
  context->buf = static_cast<char*>(buffer);
  context->action = ReceivingFrom;
  context->flags = flags;
  context->address_length = 64;
  context->address = static_cast<sockaddr*>(::malloc(context->address_length));
  context->event = event;
  ::ResetEvent(event);

  BOOL succeeded = ::QueueUserWorkItem(AsyncWork, context,
                                       WT_EXECUTEINIOTHREAD);
  if (!succeeded) {
    DestroyAsyncContext(context);
    return NULL;
  }

  return context;
}

int AsyncDatagramSocket::EndReceiveFrom(OVERLAPPED* overlapped,
                                        sockaddr* address, int* length) {
  AsyncContext* context = static_cast<AsyncContext*>(overlapped);
  if (context->action != ReceivingFrom || context->socket != this)
    return SOCKET_ERROR;

  if (context->event != NULL)
    ::WaitForSingleObject(context->event, INFINITE);

  DWORD bytes = 0;
  BOOL succeeded = ::GetOverlappedResult(*context->socket, context, &bytes,
                                         FALSE);
  if (succeeded && address != NULL && length != NULL) {
    if (*length >= context->address_length)
      ::memmove(address, context->address, context->address_length);
    *length = context->address_length;
  }

  DestroyAsyncContext(context);

  if (succeeded)
    return bytes;
  else
    return SOCKET_ERROR;
}

bool AsyncDatagramSocket::SendToAsync(const void* buffer, int size, int flags,
                                      const addrinfo* end_point,
                                      Listener* listener) {
  if (listener == NULL)
    return false;
  if (!IsValid())
    return false;

  AsyncContext* context = CreateAsyncContext();
  if (context == NULL)
    return false;

  context->len = size;
  context->buf = static_cast<char*>(const_cast<void*>(buffer));
  context->action = SendingTo;
  context->flags = flags;
  context->address_length = end_point->ai_addrlen;
  context->address = static_cast<sockaddr*>(::malloc(context->address_length));
  ::memmove(context->address, end_point->ai_addr, context->address_length);
  context->listener = listener;

  BOOL succeeded = ::QueueUserWorkItem(AsyncWork, context,
                                       WT_EXECUTEINIOTHREAD);
  if (!succeeded) {
    DestroyAsyncContext(context);
    return false;
  }

  return true;
}

OVERLAPPED* AsyncDatagramSocket::BeginSendTo(const void* buffer,
                                             int size,
                                             int flags,
                                             const addrinfo* end_point,
                                             HANDLE event) {
  if (event == NULL)
    return NULL;
  if (!IsValid())
    return NULL;

  AsyncContext* context = CreateAsyncContext();
  if (context == NULL)
    return NULL;

  context->len = size;
  context->buf = static_cast<char*>(const_cast<void*>(buffer));
  context->action = SendingTo;
  context->flags = flags;
  context->address_length = end_point->ai_addrlen;
  context->address = static_cast<sockaddr*>(::malloc(context->address_length));
  ::memmove(context->address, end_point->ai_addr, context->address_length);
  context->event = event;
  ::ResetEvent(event);

  BOOL succeeded = ::QueueUserWorkItem(AsyncWork, context,
                                       WT_EXECUTEINIOTHREAD);
  if (!succeeded) {
    DestroyAsyncContext(context);
    return NULL;
  }

  return context;
}

int AsyncDatagramSocket::EndSendTo(OVERLAPPED* overlapped) {
  AsyncContext* context = static_cast<AsyncContext*>(overlapped);
  if (context->action != SendingTo || context->socket != this)
    return SOCKET_ERROR;

  if (context->event != NULL)
    ::WaitForSingleObject(context->event, INFINITE);

  DWORD bytes = 0;
  BOOL succeeded = ::GetOverlappedResult(*context->socket, context, &bytes,
                                         FALSE);
  DestroyAsyncContext(context);

  if (succeeded)
    return bytes;
  else
    return SOCKET_ERROR;
}

bool AsyncDatagramSocket::Init() {
  if (!::BindIoCompletionCallback(*this, OnTransferred, 0))
    return false;

  return true;
}

AsyncDatagramSocket::AsyncContext* AsyncDatagramSocket::CreateAsyncContext() {
  AsyncContext* context = new AsyncContext();
  if (context == NULL)
    return NULL;

  ::memset(context, 0, sizeof(context));

  context->socket = this;

  return context;
}

void AsyncDatagramSocket::DestroyAsyncContext(AsyncContext* context) {
  if (context->address != NULL) {
    ::free(context->address);
    context->address = NULL;
  }

  delete context;
}

DWORD CALLBACK AsyncDatagramSocket::AsyncWork(void* param) {
  AsyncContext* context = static_cast<AsyncContext*>(param);

  int result = 0;
  int error = ERROR_SUCCESS;

  if (context->action == Receiving) {
    result = ::WSARecv(*context->socket, context, 1, NULL, &context->flags,
                       context, NULL);
    error = ::WSAGetLastError();
  } else if (context->action == Sending) {
    result = ::WSASend(*context->socket, context, 1, NULL, context->flags,
                       context, NULL);
    error = ::WSAGetLastError();
  } else if (context->action == ReceivingFrom) {
    result = ::WSARecvFrom(*context->socket, context, 1, NULL, &context->flags,
                           context->address, &context->address_length, context,
                           NULL);
    error = ::WSAGetLastError();
  } else if (context->action == SendingTo) {
    result = ::WSASendTo(*context->socket, context, 1, NULL, context->flags,
                         context->address, context->address_length, context,
                         NULL);
    error = ::WSAGetLastError();
  } else {
    assert(false);
  }

  if (result != 0 && error != WSA_IO_PENDING) {
    context->error = error;
    OnTransferred(error, 0, context);
  }

  return 0;
}

void CALLBACK AsyncDatagramSocket::OnTransferred(DWORD error, DWORD bytes,
                                                 OVERLAPPED* overlapped) {
  AsyncContext* context = static_cast<AsyncContext*>(overlapped);
  AsyncDatagramSocket* socket = context->socket;
  Listener* listener = context->listener;

  if (context->error != 0)
    error = context->error;

  if (listener != NULL) {
    if (context->action == Receiving) {
      socket->EndReceive(overlapped);
      listener->OnReceived(socket, error, bytes);
    } else if (context->action == Sending) {
      socket->EndSend(overlapped);
      listener->OnSent(socket, error, bytes);
    } else if (context->action == ReceivingFrom) {
      int from_length = context->address_length;
      sockaddr* from = context->address;
      context->address = NULL;
      socket->EndReceiveFrom(overlapped, NULL, NULL);

      listener->OnReceivedFrom(socket, error, bytes, from, from_length);
      ::free(from);
    } else if (context->action == SendingTo) {
      int to_length = context->address_length;
      sockaddr* to = context->address;
      context->address = NULL;
      socket->EndSendTo(overlapped);

      listener->OnSentTo(socket, error, bytes, to, to_length);
      ::free(to);
    } else {
      assert(false);
    }
  } else if (context->event != NULL) {
    ::SetEvent(context->event);
  } else {
    assert(false);
  }
}
