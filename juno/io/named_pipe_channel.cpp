// Copyright (c) 2017 dacci.org

#include "io/named_pipe_channel.h"

#include <base/logging.h>

#include <base/memory/ptr_util.h>

#include <string>

namespace juno {
namespace io {
namespace {

enum class Command {
  kInvalid,
  kConnectAsync,
  kReadAsync,
  kWriteAsync,
  kTransactAsync,
  kNotify
};

const std::wstring kPipeNamePrefix(L"\\\\.\\pipe\\");

}  // namespace

struct NamedPipeChannel::Request : OVERLAPPED {
  Command command;
  void* input;
  int input_length;
  void* output;
  int output_length;
  Channel::Listener* channel_listener;
  Listener* listener;

  Command completed_command;
  HRESULT result;
  DWORD length;
};

NamedPipeChannel::NamedPipeChannel()
    : work_(CreateThreadpoolWork(OnRequested, this, nullptr)),
      handle_(INVALID_HANDLE_VALUE),
      io_(nullptr),
      end_point_(EndPoint::kUnknown) {}

NamedPipeChannel::~NamedPipeChannel() {
  NamedPipeChannel::Close();

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

HRESULT NamedPipeChannel::Create(const base::StringPiece16& name, DWORD mode,
                                 DWORD max_instances, DWORD default_timeout) {
  // drop unsupported flags
  mode &= ~(PIPE_NOWAIT | PIPE_REJECT_REMOTE_CLIENTS);

  base::AutoLock guard(lock_);

  if (work_ == nullptr || handle_ != INVALID_HANDLE_VALUE || io_ != nullptr)
    return E_HANDLE;

  auto pipe_name = kPipeNamePrefix + name.as_string();

  handle_ = CreateNamedPipe(pipe_name.c_str(),
                            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, mode,
                            max_instances, 0, 0, default_timeout, nullptr);
  if (handle_ == INVALID_HANDLE_VALUE)
    return HRESULT_FROM_WIN32(GetLastError());

  io_ = CreateThreadpoolIo(handle_, OnCompleted, this, nullptr);
  if (io_ == nullptr)
    return HRESULT_FROM_WIN32(GetLastError());

  end_point_ = EndPoint::kServer;

  return S_OK;
}

HRESULT NamedPipeChannel::ConnectAsync(Listener* listener) {
  if (listener == nullptr)
    return E_INVALIDARG;

  auto request = std::make_unique<Request>();
  if (request == nullptr)
    return E_OUTOFMEMORY;

  memset(request.get(), 0, sizeof(*request));
  request->command = Command::kConnectAsync;
  request->listener = listener;

  base::AutoLock guard(lock_);

  if (end_point_ != EndPoint::kServer)
    return E_HANDLE;

  return DispatchRequest(std::move(request));
}

HRESULT NamedPipeChannel::Disconnect() {
  base::AutoLock guard(lock_);

  if (end_point_ != EndPoint::kServer)
    return E_HANDLE;

  if (!DisconnectNamedPipe(handle_))
    return HRESULT_FROM_WIN32(GetLastError());

  return S_OK;
}

HRESULT NamedPipeChannel::Impersonate() {
  base::AutoLock guard(lock_);

  if (end_point_ != EndPoint::kServer)
    return E_HANDLE;

  if (!ImpersonateNamedPipeClient(handle_))
    return HRESULT_FROM_WIN32(GetLastError());

  return S_OK;
}

HRESULT NamedPipeChannel::Open(const base::StringPiece16& name, DWORD timeout) {
  base::AutoLock guard(lock_);

  if (work_ == nullptr || handle_ != INVALID_HANDLE_VALUE || io_ != nullptr)
    return E_HANDLE;

  auto pipe_name = kPipeNamePrefix + name.as_string();

  if (!WaitNamedPipe(pipe_name.c_str(), timeout))
    return HRESULT_FROM_WIN32(GetLastError());

  handle_ = CreateFile(pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
  if (handle_ == INVALID_HANDLE_VALUE)
    return HRESULT_FROM_WIN32(GetLastError());

  io_ = CreateThreadpoolIo(handle_, OnCompleted, this, nullptr);
  if (io_ == nullptr)
    return HRESULT_FROM_WIN32(GetLastError());

  end_point_ = EndPoint::kClient;

  return S_OK;
}

void NamedPipeChannel::Close() {
  base::AutoLock guard(lock_);

  if (handle_ != INVALID_HANDLE_VALUE) {
    CloseHandle(handle_);
    handle_ = INVALID_HANDLE_VALUE;
    end_point_ = EndPoint::kUnknown;
  }

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

HRESULT NamedPipeChannel::ReadAsync(void* buffer, int length,
                                    Channel::Listener* listener) {
  if (buffer == nullptr && length != 0 || length < 0 || listener == nullptr)
    return E_INVALIDARG;

  auto request = std::make_unique<Request>();
  if (request == nullptr)
    return E_OUTOFMEMORY;

  memset(request.get(), 0, sizeof(*request));
  request->command = Command::kReadAsync;
  request->output = buffer;
  request->output_length = length;
  request->channel_listener = listener;

  base::AutoLock guard(lock_);

  if (!IsValid())
    return E_HANDLE;

  return DispatchRequest(std::move(request));
}

HRESULT NamedPipeChannel::WriteAsync(const void* buffer, int length,
                                     Channel::Listener* listener) {
  if (buffer == nullptr && length != 0 || length < 0 || listener == nullptr)
    return E_INVALIDARG;

  auto request = std::make_unique<Request>();
  if (request == nullptr)
    return E_OUTOFMEMORY;

  memset(request.get(), 0, sizeof(*request));
  request->command = Command::kWriteAsync;
  request->input = const_cast<void*>(buffer);
  request->input_length = length;
  request->channel_listener = listener;

  base::AutoLock guard(lock_);

  if (!IsValid())
    return E_HANDLE;

  return DispatchRequest(std::move(request));
}

HRESULT NamedPipeChannel::TransactAsync(const void* input, int input_length,
                                        void* output, int output_length,
                                        Listener* listener) {
  if (input == nullptr && input_length != 0 || input_length < 0 ||
      output == nullptr && output_length != 0 || output_length < 0 ||
      listener == nullptr)
    return E_INVALIDARG;

  auto request = std::make_unique<Request>();
  if (request == nullptr)
    return E_OUTOFMEMORY;

  memset(request.get(), 0, sizeof(*request));
  request->command = Command::kTransactAsync;
  request->input = const_cast<void*>(input);
  request->input_length = input_length;
  request->output = output;
  request->output_length = output_length;
  request->listener = listener;

  base::AutoLock guard(lock_);

  if (!IsValid())
    return E_HANDLE;

  return DispatchRequest(std::move(request));
}

HRESULT NamedPipeChannel::Read(void* buffer, int* length) {
  if (buffer != nullptr) {
    if (length == nullptr)
      return E_POINTER;

    if (*length < 0)
      return E_INVALIDARG;
  } else if (length != nullptr && *length != 0) {
    return E_INVALIDARG;
  }

  base::AutoLock guard(lock_);

  if (!IsValid())
    return E_HANDLE;

  DWORD read = length != nullptr ? *length : 0;
  auto succeeded = ReadFile(handle_, buffer, read, &read, nullptr);
  if (length != nullptr)
    *length = read;

  if (!succeeded) {
    auto error = GetLastError();
    if (error != ERROR_MORE_DATA)
      return HRESULT_FROM_WIN32(error);

    return error;
  }

  return S_OK;
}

HRESULT NamedPipeChannel::Peek(void* buffer, int* length, DWORD* total_avail,
                               DWORD* remaining) {
  if (buffer != nullptr) {
    if (length == nullptr)
      return E_POINTER;

    if (*length < 0)
      return E_INVALIDARG;
  } else if (length != nullptr && *length != 0) {
    return E_INVALIDARG;
  }

  base::AutoLock guard(lock_);

  if (!IsValid())
    return E_HANDLE;

  DWORD read = length != nullptr ? *length : 0;
  auto succeeded =
      PeekNamedPipe(handle_, buffer, read, &read, total_avail, remaining);
  if (length != nullptr)
    *length = read;

  if (!succeeded)
    return HRESULT_FROM_WIN32(GetLastError());

  return S_OK;
}

HRESULT NamedPipeChannel::GetMode(DWORD* mode) const {
  if (mode == nullptr)
    return E_POINTER;

  return GetHandleState(mode, nullptr, nullptr, 0);
}

HRESULT NamedPipeChannel::SetMode(DWORD mode) {
  // drop unsupported flag
  mode &= ~PIPE_NOWAIT;

  base::AutoLock guard(lock_);

  if (!IsValid())
    return E_HANDLE;

  if (!SetNamedPipeHandleState(handle_, &mode, nullptr, nullptr))
    return HRESULT_FROM_WIN32(GetLastError());

  return S_OK;
}

HRESULT NamedPipeChannel::GetCurrentInstances(DWORD* count) const {
  if (count == nullptr)
    return E_POINTER;

  return GetHandleState(nullptr, count, nullptr, 0);
}

HRESULT NamedPipeChannel::GetMaxInstances(DWORD* count) const {
  if (count == nullptr)
    return E_POINTER;

  if (!IsValid())
    return E_HANDLE;

  if (!GetNamedPipeInfo(handle_, nullptr, nullptr, nullptr, count))
    return HRESULT_FROM_WIN32(GetLastError());

  return S_OK;
}

HRESULT NamedPipeChannel::GetClientUserName(wchar_t* user_name,
                                            DWORD user_name_size) const {
  if (user_name == nullptr)
    return E_POINTER;

  if (end_point_ != EndPoint::kServer)
    return E_HANDLE;

  return GetHandleState(nullptr, nullptr, user_name, user_name_size);
}

HRESULT NamedPipeChannel::DispatchRequest(std::unique_ptr<Request>&& request) {
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

HRESULT NamedPipeChannel::GetHandleState(DWORD* mode, DWORD* instances,
                                         wchar_t* user_name,
                                         DWORD user_name_size) const {
  if (!IsValid())
    return E_HANDLE;

  if (!GetNamedPipeHandleState(handle_, mode, instances, nullptr, nullptr,
                               user_name, user_name_size))
    return HRESULT_FROM_WIN32(GetLastError());

  return S_OK;
}

void NamedPipeChannel::OnRequested(PTP_CALLBACK_INSTANCE callback,
                                   void* context, PTP_WORK work) {
  CallbackMayRunLong(callback);
  static_cast<NamedPipeChannel*>(context)->OnRequested(work);
}

void NamedPipeChannel::OnRequested(PTP_WORK work) {
  lock_.Acquire();

  auto request = std::move(queue_.front());
  queue_.pop();
  if (!queue_.empty())
    SubmitThreadpoolWork(work);

  do {
    base::AutoLock guard(lock_, base::AutoLock::AlreadyAcquired());

    if (request->command == Command::kNotify)
      break;

    if (handle_ == INVALID_HANDLE_VALUE || io_ == nullptr) {
      request->result = E_HANDLE;
      break;
    }

    StartThreadpoolIo(io_);
    bool succeeded;

    switch (request->command) {
      case Command::kConnectAsync:
        succeeded = ConnectNamedPipe(handle_, request.get()) != FALSE;
        break;

      case Command::kReadAsync:
        succeeded = ReadFile(handle_, request->output, request->output_length,
                             &request->length, request.get()) != FALSE;
        break;

      case Command::kWriteAsync:
        succeeded = WriteFile(handle_, request->input, request->input_length,
                              &request->length, request.get()) != FALSE;
        break;

      case Command::kTransactAsync:
        succeeded =
            TransactNamedPipe(handle_, request->input, request->input_length,
                              request->output, request->output_length,
                              &request->length, request.get()) != FALSE;
        break;

      default:
        LOG(FATAL) << "Invalid command: " << static_cast<int>(request->command);
        succeeded = false;
        SetLastError(HRESULT_CODE(E_UNEXPECTED));
        break;
    }

    auto error = GetLastError();
    if (succeeded || error == ERROR_IO_PENDING || error == ERROR_MORE_DATA) {
      request.release();
      return;
    }

    request->result = HRESULT_FROM_WIN32(error);
    CancelThreadpoolIo(io_);
  } while (false);

  if (request->command != Command::kNotify) {
    request->completed_command = request->command;
    request->command = Command::kNotify;
  }

  switch (request->completed_command) {
    case Command::kConnectAsync:
      if (HRESULT_CODE(request->result) == ERROR_PIPE_CONNECTED)
        request->result = HRESULT_CODE(request->result);

      request->listener->OnConnected(this, request->result);
      break;

    case Command::kReadAsync:
      if (HRESULT_CODE(request->result) == ERROR_MORE_DATA)
        request->result = HRESULT_CODE(request->result);

      request->channel_listener->OnRead(this, request->result, request->output,
                                        request->length);
      break;

    case Command::kWriteAsync:
      request->channel_listener->OnWritten(this, request->result,
                                           request->input, request->length);
      break;

    case Command::kTransactAsync:
      if (HRESULT_CODE(request->result) == ERROR_MORE_DATA)
        request->result = HRESULT_CODE(request->result);

      request->listener->OnTransacted(this, request->result, request->input,
                                      request->output, request->length);
      break;

    default:
      LOG(FATAL) << "Invalid command: "
                 << static_cast<int>(request->completed_command);
      break;
  }
}

void NamedPipeChannel::OnCompleted(PTP_CALLBACK_INSTANCE /*callback*/,
                                   void* context, void* overlapped, ULONG error,
                                   ULONG_PTR bytes, PTP_IO /*io*/) {
  static_cast<NamedPipeChannel*>(context)->OnCompleted(
      static_cast<OVERLAPPED*>(overlapped), error, bytes);
}

void NamedPipeChannel::OnCompleted(OVERLAPPED* overlapped, ULONG error,
                                   ULONG_PTR bytes) {
  auto request = base::WrapUnique(static_cast<Request*>(overlapped));
  request->completed_command = request->command;
  request->result = HRESULT_FROM_WIN32(error);
  request->length = static_cast<int>(bytes);
  request->command = Command::kNotify;

  base::AutoLock guard(lock_);
  auto result = DispatchRequest(std::move(request));
  LOG_IF(FATAL, FAILED(result)) << "Unrecoverable Error: 0x" << std::hex
                                << result;
}

}  // namespace io
}  // namespace juno
