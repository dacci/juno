// Copyright (c) 2016 dacci.org

#include "io/net/socket_channel.h"

#include <mswsock.h>

#include <base/logging.h>
#include <base/memory/ptr_util.h>

namespace juno {
namespace io {
namespace net {
namespace {

enum class Command {
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
    command = Command::kMonitorConnection;
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
  SocketChannel::Close();

  base::AutoLock guard(lock_);

  if (work_ != nullptr) {
    auto local_work = work_;
    work_ = nullptr;

    {
      base::AutoUnlock unlock(lock_);
      WaitForThreadpoolWorkCallbacks(local_work, FALSE);
    }

    CloseThreadpoolWork(local_work);
  }
}

void SocketChannel::Close() {
  if (connected_)
    Shutdown(SD_BOTH);

  base::AutoLock guard(lock_);

  abort_ = true;
  Socket::Close();

  if (io_ != nullptr) {
    auto local_io = io_;
    io_ = nullptr;

    {
      base::AutoUnlock unlock(lock_);
      WaitForThreadpoolIoCallbacks(local_io, FALSE);
    }

    CloseThreadpoolIo(local_io);
  }
}

HRESULT SocketChannel::ReadAsync(void* buffer, int length,
                                 Channel::Listener* listener) {
  if (buffer == nullptr && length != 0 || length < 0 || listener == nullptr)
    return E_INVALIDARG;

  auto request = std::make_unique<Request>();
  if (request == nullptr)
    return E_OUTOFMEMORY;

  memset(request.get(), 0, sizeof(*request));
  request->len = length;
  request->buf = static_cast<char*>(buffer);
  request->command = Command::kReadAsync;
  request->channel_listener = listener;

  base::AutoLock guard(lock_);

  if (!connected_)
    return HRESULT_FROM_WIN32(WSAENOTCONN);

  return DispatchRequest(std::move(request));
}

HRESULT SocketChannel::WriteAsync(const void* buffer, int length,
                                  Channel::Listener* listener) {
  if (buffer == nullptr && length != 0 || length < 0 || listener == nullptr)
    return E_INVALIDARG;

  auto request = std::make_unique<Request>();
  if (request == nullptr)
    return E_OUTOFMEMORY;

  memset(request.get(), 0, sizeof(*request));
  request->len = length;
  request->buf = const_cast<char*>(static_cast<const char*>(buffer));
  request->command = Command::kWriteAsync;
  request->channel_listener = listener;

  base::AutoLock guard(lock_);

  if (!connected_)
    return HRESULT_FROM_WIN32(WSAENOTCONN);

  return DispatchRequest(std::move(request));
}

HRESULT SocketChannel::ConnectAsync(const addrinfo* end_point,
                                    Listener* listener) {
  if (end_point == nullptr || listener == nullptr)
    return E_INVALIDARG;

  if (connected_)
    return HRESULT_FROM_WIN32(WSAEISCONN);

  if (ConnectEx == nullptr)
    return E_HANDLE;

  auto request = std::make_unique<Request>();
  if (request == nullptr)
    return E_OUTOFMEMORY;

  memset(request.get(), 0, sizeof(*request));
  request->command = Command::kConnectAsync;
  request->end_point = end_point;
  request->listener = listener;

  base::AutoLock guard(lock_);

  abort_ = false;

  return DispatchRequest(std::move(request));
}

HRESULT SocketChannel::MonitorConnection(Listener* listener) {
  if (listener == nullptr)
    return E_INVALIDARG;

  auto request = std::make_unique<Monitor>(listener);
  if (request == nullptr)
    return E_OUTOFMEMORY;

  base::AutoLock guard(lock_);

  if (!connected_)
    return HRESULT_FROM_WIN32(WSAENOTCONN);

  return DispatchRequest(std::move(request));
}

HRESULT SocketChannel::DispatchRequest(std::unique_ptr<Request>&& request) {
  lock_.AssertAcquired();

  try {
    if (work_ == nullptr)
      return E_HANDLE;

    queue_.push(std::move(request));
    if (queue_.size() == 1)
      SubmitThreadpoolWork(work_);

    return S_OK;
  } catch (...) {
    return E_UNEXPECTED;
  }
}

BOOL SocketChannel::OnInitialize(INIT_ONCE* /*init_once*/, void* /*param*/,
                                 void** /*context*/) {
  auto result = FALSE;
  SOCKET sock;

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
  queue_.pop();
  if (!queue_.empty())
    SubmitThreadpoolWork(work);

  do {
    base::AutoLock guard(lock_, base::AutoLock::AlreadyAcquired());

    if (request->command == Command::kNotify) {
      if (request->completed_command != Command::kConnectAsync)
        break;

      if (SUCCEEDED(request->result)) {
        if (SetOption(SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0)) {
          connected_ = true;
          break;
        }

        request->result = HRESULT_FROM_WIN32(WSAGetLastError());
      }

      if (request->end_point->ai_next == nullptr)
        break;

      request->command = Command::kConnectAsync;
      request->end_point = request->end_point->ai_next;
      request->result = S_OK;
    }

    if (request->command == Command::kConnectAsync) {
      if (abort_) {
        request->result = E_ABORT;
        break;
      }

      while (request->end_point != nullptr) {
        sockaddr_storage null_address{
            static_cast<ADDRESS_FAMILY>(request->end_point->ai_family)};

        {
          base::AutoUnlock unlock(lock_);
          Create(request->end_point);
        }

        if (Bind(&null_address, request->end_point->ai_addrlen))
          break;

        request->end_point = request->end_point->ai_next;
      }

      if (!IsValid() || !bound_) {
        request->result = E_HANDLE;
        break;
      }
    }

    if (io_ == nullptr) {
      io_ = CreateThreadpoolIo(reinterpret_cast<HANDLE>(descriptor_),
                               OnCompleted, this, nullptr);
      if (io_ == nullptr) {
        request->result = HRESULT_FROM_WIN32(GetLastError());
        break;
      }
    }

    StartThreadpoolIo(io_);

    bool succeeded;
    switch (request->command) {
      case Command::kReadAsync:
      case Command::kMonitorConnection:
        succeeded = WSARecv(descriptor_, request.get(), 1, nullptr,
                            &request->flags, request.get(), nullptr) == 0;
        break;

      case Command::kWriteAsync:
        succeeded = WSASend(descriptor_, request.get(), 1, nullptr,
                            request->flags, request.get(), nullptr) == 0;
        break;

      case Command::kConnectAsync:
        succeeded = ConnectEx(descriptor_, request->end_point->ai_addr,
                              static_cast<int>(request->end_point->ai_addrlen),
                              nullptr, 0, nullptr, request.get()) != FALSE;
        break;

      default:
        LOG(FATAL) << "Invalid command: " << static_cast<int>(request->command);
        succeeded = false;
        WSASetLastError(-1);
        break;
    }

    auto error = WSAGetLastError();
    if (succeeded || error == WSA_IO_PENDING) {
      request.release();
      return;
    }

    CancelThreadpoolIo(io_);
    request->result = HRESULT_FROM_WIN32(error);
  } while (false);

  if (request->command != Command::kNotify) {
    request->completed_command = request->command;
    request->command = Command::kNotify;
  }

  switch (request->completed_command) {
    case Command::kReadAsync:
      request->channel_listener->OnRead(this, request->result, request->buf,
                                        request->len);
      break;

    case Command::kWriteAsync:
      request->channel_listener->OnWritten(this, request->result, request->buf,
                                           request->len);
      break;

    case Command::kConnectAsync:
      request->listener->OnConnected(this, request->result);
      break;

    case Command::kMonitorConnection:
      if (SUCCEEDED(request->result) && request->len > 0) {
        static_cast<Monitor*>(request.get())->Reset();
        base::AutoLock guard(lock_);
        auto result = DispatchRequest(std::move(request));
        LOG_IF(WARNING, FAILED(result)) << "Unrecoverable Error: 0x" << std::hex
                                        << result;
      } else {
        request->listener->OnClosed(this, request->result);
      }
      break;

    default:
      LOG(FATAL) << "Invalid command: "
                 << static_cast<int>(request->completed_command);
      break;
  }
}

void SocketChannel::OnCompleted(PTP_CALLBACK_INSTANCE /*callback*/,
                                void* context, void* overlapped, ULONG error,
                                ULONG_PTR bytes, PTP_IO /*io*/) {
  static_cast<SocketChannel*>(context)->OnCompleted(
      static_cast<OVERLAPPED*>(overlapped), error, bytes);
}

void SocketChannel::OnCompleted(OVERLAPPED* overlapped, ULONG error,
                                ULONG_PTR bytes) {
  auto request = base::WrapUnique(static_cast<Request*>(overlapped));
  request->len = static_cast<ULONG>(bytes);
  request->completed_command = request->command;
  request->command = Command::kNotify;
  request->result = HRESULT_FROM_WIN32(error);

  base::AutoLock guard(lock_);
  auto result = DispatchRequest(std::move(request));
  LOG_IF(FATAL, FAILED(result)) << "Unrecoverable Error: 0x" << std::hex
                                << result;
}

}  // namespace net
}  // namespace io
}  // namespace juno
