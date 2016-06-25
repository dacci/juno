// Copyright (c) 2016 dacci.org

#include "net/socket_channel.h"

#include <mswsock.h>

#include <base/logging.h>

namespace {

enum Command {
  kInvalid,
  kReadAsync,
  kWriteAsync,
  kConnectAsync,
  kMonitorConnection,
  kNotify,
};

LPFN_CONNECTEX ConnectEx = nullptr;

}  // namespace

struct SocketChannel::Request : OVERLAPPED, WSABUF {
  DWORD flags;
  Command command;
  const addrinfo* end_point;
  Channel::Listener* channel_listener;
  Listener* listener;
  Command completed_command;
  HRESULT result;
};

class SocketChannel::Monitor : public Request {
 public:
  explicit Monitor(Listener* listener) : Request() {
    buf = buffer_;
    this->listener = listener;

    Reset();
  }

  void Reset() {
    len = sizeof(buffer_);
    flags = MSG_PEEK;
    command = kMonitorConnection;
  }

  char buffer_[16];
};

INIT_ONCE SocketChannel::init_once_ = INIT_ONCE_STATIC_INIT;

SocketChannel::SocketChannel()
    : work_(CreateThreadpoolWork(OnRequested, this, nullptr)),
      io_(nullptr),
      abort_(false) {
  InitOnceExecuteOnce(&init_once_, OnInitialize, nullptr, nullptr);
}

SocketChannel::~SocketChannel() {
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

void SocketChannel::Close() {
  if (connected_)
    Shutdown(SD_BOTH);

  base::AutoLock guard(lock_);

  Socket::Close();

  if (io_ != nullptr) {
    {
      base::AutoUnlock unlock(lock_);
      WaitForThreadpoolIoCallbacks(io_, FALSE);
    }

    if (io_ != nullptr) {
      CloseThreadpoolIo(io_);
      io_ = nullptr;
    }
  }
}

HRESULT SocketChannel::ReadAsync(void* buffer, int length,
                                 Channel::Listener* listener) {
  if (buffer == nullptr && length != 0 || length < 0 || listener == nullptr)
    return E_INVALIDARG;

  try {
    auto request = std::make_unique<Request>();
    if (request == nullptr)
      return E_OUTOFMEMORY;

    memset(request.get(), 0, sizeof(*request));
    request->len = length;
    request->buf = static_cast<char*>(buffer);
    request->command = kReadAsync;
    request->channel_listener = listener;

    base::AutoLock guard(lock_);

    if (work_ == nullptr || !IsValid() || !connected_)
      return E_HANDLE;

    queue_.push_back(std::move(request));
    if (queue_.size() == 1)
      SubmitThreadpoolWork(work_);
  } catch (...) {
    return E_FAIL;
  }

  return S_OK;
}

HRESULT SocketChannel::WriteAsync(const void* buffer, int length,
                                  Channel::Listener* listener) {
  if (buffer == nullptr && length != 0 || length < 0 || listener == nullptr)
    return E_INVALIDARG;

  try {
    auto request = std::make_unique<Request>();
    if (request == nullptr)
      return E_OUTOFMEMORY;

    memset(request.get(), 0, sizeof(*request));
    request->len = length;
    request->buf = const_cast<char*>(static_cast<const char*>(buffer));
    request->command = kWriteAsync;
    request->channel_listener = listener;

    base::AutoLock guard(lock_);

    if (work_ == nullptr || !IsValid() || !connected_)
      return E_HANDLE;

    queue_.push_back(std::move(request));
    if (queue_.size() == 1)
      SubmitThreadpoolWork(work_);
  } catch (...) {
    return E_FAIL;
  }

  return S_OK;
}

HRESULT SocketChannel::ConnectAsync(const addrinfo* end_point,
                                    Listener* listener) {
  if (end_point == nullptr || listener == nullptr)
    return E_INVALIDARG;

  if (connected_)
    return HRESULT_FROM_WIN32(WSAEISCONN);

  if (ConnectEx == nullptr)
    return E_HANDLE;

  try {
    auto request = std::make_unique<Request>();
    if (request == nullptr)
      return E_OUTOFMEMORY;

    memset(request.get(), 0, sizeof(*request));
    request->command = kConnectAsync;
    request->end_point = end_point;
    request->listener = listener;

    base::AutoLock guard(lock_);

    if (work_ == nullptr)
      return E_HANDLE;

    abort_ = false;

    queue_.push_back(std::move(request));
    if (queue_.size() == 1)
      SubmitThreadpoolWork(work_);
  } catch (...) {
    return E_FAIL;
  }

  return S_OK;
}

HRESULT SocketChannel::MonitorConnection(Listener* listener) {
  if (listener == nullptr)
    return E_INVALIDARG;

  try {
    auto request = std::make_unique<Monitor>(listener);
    if (request == nullptr)
      return E_OUTOFMEMORY;

    base::AutoLock guard(lock_);

    if (work_ == nullptr || !IsValid() || !connected_)
      return E_HANDLE;

    queue_.push_back(std::move(request));
    if (queue_.size() == 1)
      SubmitThreadpoolWork(work_);
  } catch (...) {
    return E_FAIL;
  }

  return S_OK;
}

BOOL SocketChannel::OnInitialize(INIT_ONCE* /*init_once*/, void* /*param*/,
                                 void** /*context*/) {
  auto result = FALSE;
  auto sock = INVALID_SOCKET;

  do {
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
      break;

    GUID guid = WSAID_CONNECTEX;
    DWORD bytes = 0;
    result =
        WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
                 &ConnectEx, sizeof(ConnectEx), &bytes, nullptr, nullptr) == 0;
  } while (false);

  if (sock != INVALID_SOCKET) {
    closesocket(sock);
    sock = INVALID_SOCKET;
  }

  return result;
}

void SocketChannel::OnRequested(PTP_CALLBACK_INSTANCE callback, void* context,
                                PTP_WORK work) {
  CallbackMayRunLong(callback);
  static_cast<SocketChannel*>(context)->OnRequested(work);
}

void SocketChannel::OnRequested(PTP_WORK work) {
  lock_.Acquire();

  auto request = std::move(queue_.front());
  queue_.pop_front();
  if (!queue_.empty())
    SubmitThreadpoolWork(work);

  if (request->command == kNotify &&
      request->completed_command == kConnectAsync && FAILED(request->result) &&
      request->end_point->ai_next != nullptr) {
    request->command = kConnectAsync;
    request->end_point = request->end_point->ai_next;
    request->completed_command = kInvalid;
    request->result = S_OK;
  }

  if (request->command != kNotify) {
    if (request->command == kConnectAsync && !abort_) {
      while (request->end_point != nullptr) {
        sockaddr_storage null_address{
            static_cast<ADDRESS_FAMILY>(request->end_point->ai_family)};

        base::AutoUnlock unlock(lock_);

        if (Create(request->end_point->ai_family,
                   request->end_point->ai_socktype,
                   request->end_point->ai_protocol) &&
            Bind(&null_address, request->end_point->ai_addrlen))
          break;

        request->end_point = request->end_point->ai_next;
      }
    }

    if (abort_) {
      request->result = E_ABORT;
    } else if (!IsValid()) {
      request->result = E_HANDLE;
    } else if (io_ == nullptr) {
      io_ = CreateThreadpoolIo(reinterpret_cast<HANDLE>(descriptor_),
                               OnCompleted, this, nullptr);
      if (io_ == nullptr)
        request->result = HRESULT_FROM_WIN32(GetLastError());
    }

    if (request->result != S_OK) {
      request->completed_command = request->command;
      request->command = kNotify;
    }
  } else if (request->completed_command == kConnectAsync &&
             SUCCEEDED(request->result)) {
    if (SetOption(SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0))
      connected_ = true;
    else
      request->result = HRESULT_FROM_WIN32(WSAGetLastError());
  }

  lock_.Release();

  if (request->command != kNotify && request->result == S_OK) {
    auto succeeded = false;

    StartThreadpoolIo(io_);

    switch (request->command) {
      case kReadAsync:
      case kMonitorConnection:
        succeeded = WSARecv(descriptor_, request.get(), 1, nullptr,
                            &request->flags, request.get(), nullptr) == 0;
        break;

      case kWriteAsync:
        succeeded = WSASend(descriptor_, request.get(), 1, nullptr,
                            request->flags, request.get(), nullptr) == 0;
        break;

      case kConnectAsync:
        succeeded = ConnectEx(descriptor_, request->end_point->ai_addr,
                              static_cast<int>(request->end_point->ai_addrlen),
                              nullptr, 0, nullptr, request.get()) != FALSE;
        break;

      default:
        DCHECK(false) << "This must not occur.";
    }

    auto error = WSAGetLastError();
    if (succeeded || error == WSA_IO_PENDING) {
      request.release();
      return;
    }

    CancelThreadpoolIo(io_);
    request->completed_command = request->command;
    request->command = kNotify;
    request->result = HRESULT_FROM_WIN32(error);
  }

  switch (request->completed_command) {
    case kReadAsync:
      request->channel_listener->OnRead(this, request->result, request->buf,
                                        request->len);
      break;

    case kWriteAsync:
      request->channel_listener->OnWritten(this, request->result, request->buf,
                                           request->len);
      break;

    case kConnectAsync:
      request->listener->OnConnected(this, request->result);
      break;

    case kMonitorConnection:
      if (SUCCEEDED(request->result) && request->len > 0) {
        static_cast<Monitor*>(request.get())->Reset();

        base::AutoLock guard(lock_);

        queue_.push_back(std::move(request));
        if (queue_.size() == 1)
          SubmitThreadpoolWork(work);
      } else {
        request->listener->OnClosed(this, request->result);
      }
      break;

    default:
      DCHECK(false) << "This must not occur.";
  }
}

void SocketChannel::OnCompleted(PTP_CALLBACK_INSTANCE /*callback*/,
                                void* context, void* overlapped, ULONG error,
                                ULONG_PTR bytes, PTP_IO /*io*/) {
  std::unique_ptr<Request> request(
      static_cast<Request*>(static_cast<OVERLAPPED*>(overlapped)));
  request->len = static_cast<ULONG>(bytes);
  request->completed_command = request->command;
  request->command = kNotify;
  request->result = HRESULT_FROM_WIN32(error);

  auto instance = static_cast<SocketChannel*>(context);
  base::AutoLock guard(instance->lock_);
  instance->queue_.push_back(std::move(request));
  if (instance->queue_.size() == 1)
    SubmitThreadpoolWork(instance->work_);
}
