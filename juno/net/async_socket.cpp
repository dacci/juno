// Copyright (c) 2013 dacci.org

#include "net/async_socket.h"

#include <assert.h>

LPFN_CONNECTEX AsyncSocket::ConnectEx = NULL;

AsyncSocket::AsyncSocket() : initialized_(), io_() {
}

AsyncSocket::~AsyncSocket() {
  Close();
}

void AsyncSocket::Close() {
  initialized_ = false;
  cancel_connect_ = true;

  Socket::Close();

  if (io_ != NULL) {
    ::WaitForThreadpoolIoCallbacks(io_, FALSE);
    ::CloseThreadpoolIo(io_);
    io_ = NULL;
  }
}

bool AsyncSocket::UpdateAcceptContext(SOCKET descriptor) {
  if (connected_)
    return false;

  if (SetOption(SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, descriptor)) {
    connected_ = true;
    return Init();
  }

  return false;
}

bool AsyncSocket::ConnectAsync(const addrinfo* end_points, Listener* listener) {
  if (end_points == NULL || listener == NULL)
    return false;
  if (connected_)
    return false;

  cancel_connect_ = false;

  AsyncContext* context = CreateAsyncContext();
  if (context == NULL)
    return false;

  context->action = Connecting;
  context->end_point = end_points;
  context->listener = listener;

  BOOL succeeded = ::TrySubmitThreadpoolCallback(AsyncWork, context, NULL);
  if (!succeeded) {
    DestroyAsyncContext(context);
    return false;
  }

  return true;
}

OVERLAPPED* AsyncSocket::BeginConnect(const addrinfo* end_points,
                                      HANDLE event) {
  if (end_points == NULL || event == NULL)
    return NULL;
  if (connected_)
    return NULL;

  cancel_connect_ = false;

  AsyncContext* context = CreateAsyncContext();
  if (context == NULL)
    return NULL;

  context->action = Connecting;
  context->end_point = end_points;
  context->event = event;
  ::ResetEvent(event);

  BOOL succeeded = ::TrySubmitThreadpoolCallback(AsyncWork, context, NULL);
  if (!succeeded) {
    DestroyAsyncContext(context);
    return NULL;
  }

  return context;
}

void AsyncSocket::EndConnect(OVERLAPPED* overlapped) {
  AsyncContext* context = static_cast<AsyncContext*>(overlapped);
  if (context->action != Connecting || context->socket != this)
    return;

  if (context->event != NULL)
    ::WaitForSingleObject(context->event, INFINITE);

  DWORD bytes = 0;
  BOOL succeeded = ::GetOverlappedResult(*context->socket, context, &bytes,
                                         FALSE);
  if (succeeded) {
    SetOption(SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
    connected_ = true;
  }

  DestroyAsyncContext(context);
}

void AsyncSocket::CancelAsyncConnect() {
  if (connected_)
    return;

  cancel_connect_ = true;
  Close();
}

bool AsyncSocket::ReceiveAsync(void* buffer, int size, int flags,
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

OVERLAPPED* AsyncSocket::BeginReceive(void* buffer, int size, int flags,
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

int AsyncSocket::EndReceive(OVERLAPPED* overlapped) {
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

bool AsyncSocket::SendAsync(const void* buffer, int size, int flags,
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

OVERLAPPED* AsyncSocket::BeginSend(const void* buffer, int size, int flags,
                                   HANDLE event) {
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

int AsyncSocket::EndSend(OVERLAPPED* overlapped) {
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

bool AsyncSocket::Init() {
  if (initialized_)
    return true;

  io_ = ::CreateThreadpoolIo(*this, OnTransferred, this, NULL);
  if (io_ == NULL)
    return false;

  initialized_ = true;

  return true;
}

AsyncSocket::AsyncContext* AsyncSocket::CreateAsyncContext() {
  if (IsValid() && !Init())
    return NULL;

  AsyncContext* context = new AsyncContext();
  if (context == NULL)
    return NULL;

  ::memset(context, 0, sizeof(*context));

  context->socket = this;

  return context;
}

void AsyncSocket::DestroyAsyncContext(AsyncContext* context) {
  delete context;
}

int AsyncSocket::DoAsyncConnect(AsyncContext* context) {
  const addrinfo* end_point = context->end_point;

  if (!Create(end_point))
    return SOCKET_ERROR;

  if (!Init())
    return SOCKET_ERROR;

  if (ConnectEx == NULL) {
    GUID guid = WSAID_CONNECTEX;
    DWORD size = 0;
    ::WSAIoctl(descriptor_, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid,
               sizeof(guid), &ConnectEx, sizeof(ConnectEx), &size, NULL, NULL);
  }

  sockaddr_storage address = { end_point->ai_family };
  int result = ::bind(descriptor_, reinterpret_cast<sockaddr*>(&address),
                      end_point->ai_addrlen);
  if (result != 0)
    return SOCKET_ERROR;

  ::StartThreadpoolIo(context->socket->io_);

  return ConnectEx(descriptor_, end_point->ai_addr, end_point->ai_addrlen, NULL,
                   0, NULL, context);
}

void CALLBACK AsyncSocket::AsyncWork(PTP_CALLBACK_INSTANCE instance,
                                     void* param) {
  AsyncContext* context = static_cast<AsyncContext*>(param);

  DWORD bytes = 0;
  int result = 0;
  int error = ERROR_SUCCESS;

  if (context->action == Connecting) {
    result = context->socket->DoAsyncConnect(context);
    error = ::WSAGetLastError();
  } else if (context->action == Receiving) {
    ::StartThreadpoolIo(context->socket->io_);
    result = ::WSARecv(*context->socket, context, 1, &bytes, &context->flags,
                       context, NULL);
    error = ::WSAGetLastError();
  } else if (context->action == Sending) {
    ::StartThreadpoolIo(context->socket->io_);
    result = ::WSASend(*context->socket, context, 1, &bytes, context->flags,
                       context, NULL);
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

void CALLBACK AsyncSocket::OnTransferred(PTP_CALLBACK_INSTANCE instance,
                                         void* self,
                                         void* overlapped,
                                         ULONG error,
                                         ULONG_PTR bytes,
                                         PTP_IO io) {
  AsyncContext* context =
      static_cast<AsyncContext*>(static_cast<OVERLAPPED*>(overlapped));
  AsyncSocket* socket = context->socket;
  Listener* listener = context->listener;

  if (context->error != 0)
    error = context->error;

  if (context->action == Connecting && error != 0) {
    if (!socket->cancel_connect_ && context->end_point->ai_next != NULL) {
      context->end_point = context->end_point->ai_next;
      BOOL succeeded = ::TrySubmitThreadpoolCallback(AsyncWork, context, NULL);
      if (succeeded)
        return;
    }
  }

  if (listener != NULL) {
    if (context->action == Connecting) {
      socket->EndConnect(context);
      listener->OnConnected(socket, error);
    } else if (context->action == Receiving) {
      socket->EndReceive(context);
      listener->OnReceived(socket, error, bytes);
    } else if (context->action == Sending) {
      socket->EndSend(context);
      listener->OnSent(socket, error, bytes);
    } else {
      assert(false);
    }
  } else if (context->event != NULL) {
    ::SetEvent(context->event);
  } else {
    assert(false);
  }
}
