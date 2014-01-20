// Copyright (c) 2013 dacci.org

#include "net/async_datagram_socket.h"

#include <assert.h>

AsyncDatagramSocket::AsyncDatagramSocket() : initialized_(), io_() {
}

AsyncDatagramSocket::~AsyncDatagramSocket() {
  Close();
}

void AsyncDatagramSocket::Close() {
  initialized_ = false;

  DatagramSocket::Close();

  if (io_ != NULL) {
    ::WaitForThreadpoolIoCallbacks(io_, FALSE);
    ::CloseThreadpoolIo(io_);
    io_ = NULL;
  }
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

  BOOL succeeded = ::TrySubmitThreadpoolCallback(AsyncWork, context, NULL);
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

  BOOL succeeded = ::TrySubmitThreadpoolCallback(AsyncWork, context, NULL);
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

  BOOL succeeded = ::TrySubmitThreadpoolCallback(AsyncWork, context, NULL);
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

  BOOL succeeded = ::TrySubmitThreadpoolCallback(AsyncWork, context, NULL);
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

  BOOL succeeded = ::TrySubmitThreadpoolCallback(AsyncWork, context, NULL);
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

  BOOL succeeded = ::TrySubmitThreadpoolCallback(AsyncWork, context, NULL);
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

  BOOL succeeded = ::TrySubmitThreadpoolCallback(AsyncWork, context, NULL);
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

  BOOL succeeded = ::TrySubmitThreadpoolCallback(AsyncWork, context, NULL);
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

AsyncDatagramSocket::AsyncContext* AsyncDatagramSocket::CreateAsyncContext() {
  if (!initialized_) {
    io_ = ::CreateThreadpoolIo(*this, OnTransferred, this, NULL);
    if (io_ == NULL)
      return NULL;

    initialized_ = true;
  }

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

void CALLBACK AsyncDatagramSocket::AsyncWork(PTP_CALLBACK_INSTANCE instance,
                                             void* param) {
  AsyncContext* context = static_cast<AsyncContext*>(param);

  DWORD bytes = 0;
  int result = 0;
  int error = ERROR_SUCCESS;

  ::StartThreadpoolIo(context->socket->io_);

  if (context->action == Receiving) {
    result = ::WSARecv(*context->socket, context, 1, &bytes, &context->flags,
                       context, NULL);
    error = ::WSAGetLastError();
  } else if (context->action == Sending) {
    result = ::WSASend(*context->socket, context, 1, &bytes, context->flags,
                       context, NULL);
    error = ::WSAGetLastError();
  } else if (context->action == ReceivingFrom) {
    result = ::WSARecvFrom(*context->socket, context, 1, &bytes,
                           &context->flags, context->address,
                           &context->address_length, context, NULL);
    error = ::WSAGetLastError();
  } else if (context->action == SendingTo) {
    result = ::WSASendTo(*context->socket, context, 1, &bytes, context->flags,
                         context->address, context->address_length, context,
                         NULL);
    error = ::WSAGetLastError();
  } else {
    assert(false);
  }

  if (result != 0 && error != WSA_IO_PENDING) {
    context->error = error;
    ::CancelThreadpoolIo(context->socket->io_);
    OnTransferred(instance, context->socket, static_cast<OVERLAPPED*>(context),
                  error, 0, context->socket->io_);
  }
}

void CALLBACK AsyncDatagramSocket::OnTransferred(PTP_CALLBACK_INSTANCE instance,
                                                 void* self,
                                                 void* overlapped,
                                                 ULONG error,
                                                 ULONG_PTR bytes,
                                                 PTP_IO io) {
  AsyncContext* context =
      static_cast<AsyncContext*>(static_cast<OVERLAPPED*>(overlapped));
  AsyncDatagramSocket* socket = context->socket;
  Listener* listener = context->listener;

  if (context->error != 0)
    error = context->error;

  if (listener != NULL) {
    if (context->action == Receiving) {
      socket->EndReceive(context);
      listener->OnReceived(socket, error, bytes);
    } else if (context->action == Sending) {
      socket->EndSend(context);
      listener->OnSent(socket, error, bytes);
    } else if (context->action == ReceivingFrom) {
      int from_length = context->address_length;
      sockaddr* from = context->address;
      context->address = NULL;
      socket->EndReceiveFrom(context, NULL, NULL);

      listener->OnReceivedFrom(socket, error, bytes, from, from_length);
      ::free(from);
    } else if (context->action == SendingTo) {
      int to_length = context->address_length;
      sockaddr* to = context->address;
      context->address = NULL;
      socket->EndSendTo(context);

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
