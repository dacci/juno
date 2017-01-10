// Copyright (c) 2016 dacci.org

#include "io/net/datagram_channel.h"

#include <base/logging.h>

#include <base/memory/ptr_util.h>

namespace juno {
namespace io {
namespace net {
namespace {

enum class Command {
  kInvalid,
  kReadAsync,
  kRecvFrom,
  kWriteAsync,
  kSendTo,
  kNotify,
};

}  // namespace

struct DatagramChannel::Request : OVERLAPPED, WSABUF {
  DWORD flags;
  sockaddr_storage address;
  int address_length;
  Command command;
  Channel::Listener* channel_listener;
  Listener* listener;

  Command completed_command;
  HRESULT result;
};

DatagramChannel::DatagramChannel()
    : work_(CreateThreadpoolWork(OnRequested, this, nullptr)), io_(nullptr) {}

DatagramChannel::~DatagramChannel() {
  DatagramChannel::Close();

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

void DatagramChannel::Close() {
  AbstractSocket::Close();

  base::AutoLock guard(lock_);

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

HRESULT DatagramChannel::ReadAsync(void* buffer, int length,
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

HRESULT DatagramChannel::ReadFromAsync(void* buffer, int length,
                                       Listener* listener) {
  if (buffer == nullptr && length != 0 || length < 0 || listener == nullptr)
    return E_INVALIDARG;

  auto request = std::make_unique<Request>();
  if (request == nullptr)
    return E_OUTOFMEMORY;

  memset(request.get(), 0, sizeof(*request));
  request->len = length;
  request->buf = static_cast<char*>(buffer);
  request->address_length = sizeof(request->address);
  request->command = Command::kRecvFrom;
  request->listener = listener;

  base::AutoLock guard(lock_);

  if (connected_)
    return HRESULT_FROM_WIN32(WSAEISCONN);

  if (!IsValid())
    return HRESULT_FROM_WIN32(WSAENOTSOCK);

  return DispatchRequest(std::move(request));
}

HRESULT DatagramChannel::WriteAsync(const void* buffer, int length,
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

HRESULT DatagramChannel::WriteAsync(const void* buffer, int length,
                                    const void* address, size_t address_length,
                                    Channel::Listener* listener) {
  if (buffer == nullptr && length != 0 || length < 0 || address == nullptr ||
      address_length > sizeof(sockaddr_storage) || listener == nullptr)
    return E_INVALIDARG;

  auto request = std::make_unique<Request>();
  if (request == nullptr)
    return E_OUTOFMEMORY;

  memset(request.get(), 0, sizeof(*request));
  request->len = length;
  request->buf = const_cast<char*>(static_cast<const char*>(buffer));
  memmove(&request->address, address, address_length);
  request->address_length = static_cast<int>(address_length);
  request->command = Command::kSendTo;
  request->channel_listener = listener;

  base::AutoLock guard(lock_);

  if (connected_)
    return HRESULT_FROM_WIN32(WSAEISCONN);

  if (!IsValid())
    return HRESULT_FROM_WIN32(WSAENOTSOCK);

  return DispatchRequest(std::move(request));
}

HRESULT DatagramChannel::DispatchRequest(std::unique_ptr<Request>&& request) {
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

void DatagramChannel::OnRequested(PTP_CALLBACK_INSTANCE callback,
                                  void* instance, PTP_WORK work) {
  CallbackMayRunLong(callback);
  static_cast<DatagramChannel*>(instance)->OnRequested(work);
}

void DatagramChannel::OnRequested(PTP_WORK work) {
  lock_.Acquire();

  auto request = std::move(queue_.front());
  queue_.pop();
  if (!queue_.empty())
    SubmitThreadpoolWork(work);

  do {
    base::AutoLock guard(lock_, base::AutoLock::AlreadyAcquired());

    if (request->command == Command::kNotify)
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

    StartThreadpoolIo(io_);

    auto succeeded = false;
    switch (request->command) {
      case Command::kReadAsync:
        succeeded = WSARecv(descriptor_, request.get(), 1, nullptr,
                            &request->flags, request.get(), nullptr) == 0;
        break;

      case Command::kRecvFrom:
        succeeded =
            WSARecvFrom(descriptor_, request.get(), 1, nullptr, &request->flags,
                        reinterpret_cast<sockaddr*>(&request->address),
                        &request->address_length, request.get(), nullptr) == 0;
        break;

      case Command::kWriteAsync:
        succeeded = WSASend(descriptor_, request.get(), 1, nullptr,
                            request->flags, request.get(), nullptr) == 0;
        break;

      case Command::kSendTo:
        succeeded =
            WSASendTo(descriptor_, request.get(), 1, nullptr, request->flags,
                      reinterpret_cast<sockaddr*>(&request->address),
                      sizeof(request->address), request.get(), nullptr) == 0;
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

    case Command::kRecvFrom:
      request->listener->OnRead(this, request->result, request->buf,
                                request->len, &request->address,
                                request->address_length);
      break;

    case Command::kWriteAsync:
    case Command::kSendTo:
      request->channel_listener->OnWritten(this, request->result, request->buf,
                                           request->len);
      break;

    default:
      LOG(FATAL) << "Invalid command: "
                 << static_cast<int>(request->completed_command);
      break;
  }
}

void DatagramChannel::OnCompleted(PTP_CALLBACK_INSTANCE /*callback*/,
                                  void* context, void* overlapped, ULONG error,
                                  ULONG_PTR bytes, PTP_IO /*io*/) {
  static_cast<DatagramChannel*>(context)->OnCompleted(
      static_cast<OVERLAPPED*>(overlapped), error, bytes);
}

void DatagramChannel::OnCompleted(OVERLAPPED* overlapped, ULONG error,
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
