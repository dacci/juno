// Copyright (c) 2016 dacci.org

#include "net/async_server_socket.h"

#include <mswsock.h>

class AsyncServerSocket::Context : public OVERLAPPED {
 public:
  Context(AsyncServerSocket* server, Listener* listener)
      : OVERLAPPED(),
        server(server),
        listener(listener),
        socket(INVALID_SOCKET),
        completed(false),
        result(S_OK) {}

  ~Context() {
    if (socket != INVALID_SOCKET) {
      closesocket(socket);
      socket = INVALID_SOCKET;
    }
  }

  AsyncServerSocket* const server;
  Listener* const listener;
  SOCKET socket;
  char buffer[kAddressBufferSize * 2];
  bool completed;
  HRESULT result;
};

AsyncServerSocket::AsyncServerSocket()
    : work_(CreateThreadpoolWork(OnRequested, this, nullptr)),
      io_(nullptr),
      protocol_() {}

AsyncServerSocket::~AsyncServerSocket() {
  Close();

  base::AutoLock guard(lock_);

  if (work_ != nullptr) {
    auto local_work = work_;
    work_ = nullptr;

    base::AutoUnlock unlock(lock_);

    WaitForThreadpoolWorkCallbacks(local_work, FALSE);
    CloseThreadpoolWork(local_work);
  }
}

void AsyncServerSocket::Close() {
  ServerSocket::Close();

  base::AutoLock guard(lock_);

  if (io_ != nullptr) {
    auto local_io = io_;
    io_ = nullptr;

    base::AutoUnlock unlock(lock_);

    WaitForThreadpoolIoCallbacks(local_io, FALSE);
    CloseThreadpoolIo(local_io);
  }
}

HRESULT AsyncServerSocket::AcceptAsync(Listener* listener) {
  if (listener == nullptr)
    return E_INVALIDARG;

  try {
    auto request = std::make_unique<Context>(this, listener);
    if (request == nullptr)
      return E_OUTOFMEMORY;

    base::AutoLock guard(lock_);

    if (work_ == nullptr || !IsValid() || !listening_)
      return E_HANDLE;

    queue_.push_back(std::move(request));

    if (queue_.size() == 1)
      SubmitThreadpoolWork(work_);
  } catch (...) {
    return E_FAIL;
  }

  return S_OK;
}

SOCKET AsyncServerSocket::EndAcceptImpl(Context* context, HRESULT* result) {
  if (context == nullptr || context->server != this || !context->completed) {
    if (result != nullptr)
      *result = E_INVALIDARG;

    return INVALID_SOCKET;
  }

  base::AutoLock guard(lock_);

  DWORD bytes = 0;
  auto succeeded = GetOverlappedResult(reinterpret_cast<HANDLE>(descriptor_),
                                       context, &bytes, TRUE) != FALSE;
  if (!succeeded) {
    if (result != nullptr)
      *result = HRESULT_FROM_WIN32(GetLastError());

    return INVALID_SOCKET;
  }

  succeeded = setsockopt(context->socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                         reinterpret_cast<char*>(&descriptor_),
                         sizeof(descriptor_)) == 0;
  if (!succeeded) {
    if (result != nullptr)
      *result = HRESULT_FROM_WIN32(WSAGetLastError());

    return INVALID_SOCKET;
  }

  auto socket = context->socket;
  context->socket = INVALID_SOCKET;

  if (result != nullptr)
    *result = S_OK;

  return socket;
}

void AsyncServerSocket::OnRequested(PTP_CALLBACK_INSTANCE callback,
                                    void* context, PTP_WORK work) {
  CallbackMayRunLong(callback);
  static_cast<AsyncServerSocket*>(context)->OnRequested(work);
}

void AsyncServerSocket::OnRequested(PTP_WORK work) {
  lock_.Acquire();

  auto request = std::move(queue_.front());
  queue_.pop_front();
  if (!queue_.empty())
    SubmitThreadpoolWork(work);

  do {
    base::AutoLock guard(lock_, base::AutoLock::AlreadyAcquired());

    if (request->completed)
      break;

    if (!IsValid()) {
      request->result = E_HANDLE;
      break;
    }

    if (io_ == nullptr) {
      io_ = CreateThreadpoolIo(reinterpret_cast<HANDLE>(descriptor_),
                               OnCompleted, this, nullptr);
      if (io_ == nullptr) {
        request->result = HRESULT_FROM_WIN32(GetLastError());
        break;
      }
    }

    if (protocol_.iAddressFamily == 0 &&
        !GetOption(SOL_SOCKET, SO_PROTOCOL_INFO, &protocol_)) {
      request->result = HRESULT_FROM_WIN32(WSAGetLastError());
      break;
    }

    request->socket = socket(protocol_.iAddressFamily, protocol_.iSocketType,
                             protocol_.iProtocol);
    if (request->socket == INVALID_SOCKET) {
      request->result = HRESULT_FROM_WIN32(WSAGetLastError());
      break;
    }

    StartThreadpoolIo(io_);

    auto succeeded = AcceptEx(descriptor_, request->socket, request->buffer, 0,
                              kAddressBufferSize, kAddressBufferSize, nullptr,
                              request.get());
    auto error = WSAGetLastError();
    if (succeeded || error == WSA_IO_PENDING) {
      request.release();
      return;
    }

    CancelThreadpoolIo(io_);
    request->result = HRESULT_FROM_WIN32(error);
  } while (false);

  request->listener->OnAccepted(this, request->result, request.get());
}

void AsyncServerSocket::OnCompleted(PTP_CALLBACK_INSTANCE /*callback*/,
                                    void* context, void* overlapped,
                                    ULONG error, ULONG_PTR /*bytes*/,
                                    PTP_IO /*io*/) {
  std::unique_ptr<Context> request(
      static_cast<Context*>(static_cast<OVERLAPPED*>(overlapped)));
  request->completed = true;
  request->result = HRESULT_FROM_WIN32(error);

  auto instance = static_cast<AsyncServerSocket*>(context);
  base::AutoLock guard(instance->lock_);
  instance->queue_.push_back(std::move(request));
  if (instance->queue_.size() == 1)
    SubmitThreadpoolWork(instance->work_);
}
