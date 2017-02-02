// Copyright (c) 2016 dacci.org

#include "io/secure_channel.h"

#include <base/logging.h>

#include <algorithm>
#include <string>
#include <vector>

namespace juno {
namespace io {
namespace {

enum class ContentType : uint8_t {
  kChangeCipherSpec = 20,
  kAlert = 21,
  kHandshake = 22,
  kApplicationData = 23,
};

#include <pshpack1.h>  // NOLINT(build/include_order)

struct TLS_RECORD {
  ContentType type;
  uint8_t major_version;
  uint8_t minor_version;
  uint16_t length;
};

#include <poppack.h>  // NOLINT(build/include_order)

class SecurityBuffer : public std::vector<SecBuffer> {
 public:
  SecurityBuffer(std::initializer_list<SecBuffer> buffers)
      : std::vector<SecBuffer>(buffers) {
    description_.ulVersion = SECBUFFER_VERSION;
    description_.cBuffers = static_cast<DWORD>(size());
    description_.pBuffers = data();
  }

  SecBufferDesc* get() {
    return &description_;
  }

 private:
  SecBufferDesc description_;
};

}  // namespace

struct SecureChannel::Request {
  void* buffer;
  int length;
  Channel::Listener* listener;
};

enum class SecureChannel::Status {
  kError = -1,
  kInit = 0,
  kNegotiate,
  kData,
  kClosing,
  kClosed
};

SecureChannel::SecureChannel(misc::schannel::SchannelCredential* credential,
                             std::unique_ptr<Channel>&& channel, bool inbound)
    : context_(credential),
      channel_(std::move(channel)),
      inbound_(inbound),
      deletable_(&lock_),
      ref_count_(0),
      status_(Status::kInit),
      stream_sizes_(),
      reads_(0),
      read_work_(CreateThreadpoolWork(OnRead, this, nullptr)),
      write_work_(CreateThreadpoolWork(OnWrite, this, nullptr)) {
  if (credential == nullptr || channel_ == nullptr || read_work_ == nullptr ||
      write_work_ == nullptr)
    status_ = Status::kError;
}

SecureChannel::~SecureChannel() {
  SecureChannel::Close();

  base::AutoLock guard(lock_);

  while (!base::AtomicRefCountIsZero(&ref_count_))
    deletable_.Wait();

  if (read_work_ != nullptr) {
    auto local_work = read_work_;
    read_work_ = nullptr;

    {
      base::AutoUnlock unlock(lock_);
      WaitForThreadpoolWorkCallbacks(local_work, FALSE);
    }

    CloseThreadpoolWork(local_work);
  }

  if (write_work_ != nullptr) {
    auto local_work = write_work_;
    write_work_ = nullptr;

    {
      base::AutoUnlock unlock(lock_);
      WaitForThreadpoolWorkCallbacks(local_work, FALSE);
    }

    CloseThreadpoolWork(local_work);
  }
}

void SecureChannel::Close() {
  if (status_ == Status::kClosed)
    return;

  do {
    base::AutoLock guard(lock_);

    auto result = context_.ApplyControlToken(SCHANNEL_SHUTDOWN);
    if (FAILED(result)) {
      status_ = Status::kError;
      LOG(ERROR) << "ApplyControlToken() failed: 0x" << std::hex << result;
      break;
    }

    status_ = Status::kClosing;
    message_.clear();

    result = Negotiate();
    if (FAILED(result)) {
      status_ = Status::kError;
      LOG(ERROR) << "Failed to begin negotiation: 0x" << std::hex << result;
      break;
    }

    result = EnsureReading();
    if (FAILED(result)) {
      status_ = Status::kError;
      LOG(ERROR) << "Failed to begin reading: 0x" << std::hex << result;
      break;
    }

    while (!base::AtomicRefCountIsZero(&ref_count_))
      deletable_.Wait();

    status_ = Status::kClosed;
  } while (false);

  channel_->Close();
}

HRESULT SecureChannel::ReadAsync(void* buffer, int length,
                                 Channel::Listener* listener) {
  if (buffer == nullptr && length != 0 || length < 0 || listener == nullptr)
    return E_INVALIDARG;

  try {
    auto request = std::make_unique<Request>();
    if (request == nullptr)
      return E_OUTOFMEMORY;

    request->buffer = buffer;
    request->length = length;
    request->listener = listener;

    base::AutoLock guard(lock_);

    if (read_work_ == nullptr)
      return E_HANDLE;

    pending_reads_.push(std::move(request));
    if (pending_reads_.size() == 1)
      SubmitThreadpoolWork(read_work_);
  } catch (...) {
    return E_FAIL;
  }

  return S_OK;
}

HRESULT SecureChannel::WriteAsync(const void* buffer, int length,
                                  Channel::Listener* listener) {
  if (buffer == nullptr && length != 0 || length < 0 || listener == nullptr)
    return E_INVALIDARG;

  try {
    auto request = std::make_unique<Request>();
    if (request == nullptr)
      return E_OUTOFMEMORY;

    request->buffer = const_cast<void*>(buffer);
    request->length = length;
    request->listener = listener;

    base::AutoLock guard(lock_);

    if (write_work_ == nullptr)
      return E_HANDLE;

    pending_writes_.push(std::move(request));
    if (pending_writes_.size() == 1)
      SubmitThreadpoolWork(write_work_);
  } catch (...) {
    return E_FAIL;
  }

  return S_OK;
}

HRESULT SecureChannel::CheckMessage() const {
  static const uint16_t kMaxLength = 16384;

  lock_.AssertAcquired();

  if (message_.size() < sizeof(TLS_RECORD))
    return SEC_E_INCOMPLETE_MESSAGE;

  auto record = reinterpret_cast<const TLS_RECORD*>(message_.data());

  if (_byteswap_ushort(record->length) > kMaxLength)
    return SEC_E_ILLEGAL_MESSAGE;

  switch (record->type) {
    case ContentType::kChangeCipherSpec:
    case ContentType::kAlert:
    case ContentType::kHandshake:
      if (record->length == 0)
        return SEC_E_ILLEGAL_MESSAGE;
      break;

    case ContentType::kApplicationData:
      break;

    default:
      return SEC_E_ILLEGAL_MESSAGE;
  }

  return S_OK;
}

HRESULT SecureChannel::EnsureInitialized() {
  lock_.AssertAcquired();

  if (status_ != Status::kInit)
    return S_FALSE;

  status_ = Status::kNegotiate;

  if (!inbound_) {
    message_.clear();

    auto result = Negotiate();
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to begin negotiation: 0x" << std::hex << result;
      return result;
    }
  }

  auto result = EnsureReading();
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to begin reading: 0x" << std::hex << result;
    return result;
  }

  return S_OK;
}

HRESULT SecureChannel::Negotiate() {
  lock_.AssertAcquired();

  HRESULT result;
  std::string response;

  do {
    SecurityBuffer inputs{
        {static_cast<DWORD>(message_.size()), SECBUFFER_TOKEN, &message_[0]},
        {0, SECBUFFER_EMPTY, nullptr},
    };
    SecurityBuffer outputs{
        {0, SECBUFFER_TOKEN, nullptr},
        {0, SECBUFFER_ALERT, nullptr},
        {0, SECBUFFER_EXTRA, nullptr},
    };

    if (inbound_)
      result = context_.AcceptContext(
          ASC_REQ_REPLAY_DETECT | ASC_REQ_SEQUENCE_DETECT |
              ASC_REQ_CONFIDENTIALITY | ASC_REQ_ALLOCATE_MEMORY |
              ASC_REQ_EXTENDED_ERROR | ASC_REQ_STREAM,
          inputs.get(), outputs.get());
    else
      result = context_.InitializeContext(
          ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT |
              ISC_REQ_CONFIDENTIALITY | ISC_REQ_ALLOCATE_MEMORY |
              ISC_REQ_EXTENDED_ERROR | ISC_REQ_STREAM,
          inputs.get(), outputs.get());

    for (auto& output : outputs) {
      if (output.pvBuffer == nullptr)
        continue;

      response.append(static_cast<char*>(output.pvBuffer), output.cbBuffer);
      FreeContextBuffer(output.pvBuffer);
    }

    if (result == SEC_E_INCOMPLETE_MESSAGE)
      break;

    message_.erase(0, message_.size() - inputs[1].cbBuffer);

    if (result == SEC_E_OK) {
      if (status_ == Status::kClosing)
        status_ = Status::kClosed;
      else
        status_ = Status::kData;
    }

    if (FAILED(result)) {
      message_.clear();
      break;
    }
  } while (!message_.empty());

  if (result == SEC_E_INCOMPLETE_MESSAGE)
    result = S_OK;
  else
    DCHECK(message_.empty()) << "Unprocessed message is left.";

  if (!response.empty())
    result = WriteAsyncImpl(response, nullptr);

  if (status_ == Status::kData)
    context_.QueryAttributes(SECPKG_ATTR_STREAM_SIZES, &stream_sizes_);

  return result;
}

HRESULT SecureChannel::Decrypt() {
  lock_.AssertAcquired();

  while (!message_.empty()) {
    SecurityBuffer buffers{
        {static_cast<DWORD>(message_.size()), SECBUFFER_DATA, &message_[0]},
        {0, SECBUFFER_EMPTY, nullptr},
        {0, SECBUFFER_EMPTY, nullptr},
        {0, SECBUFFER_EMPTY, nullptr},
    };

    auto result = context_.DecryptMessage(buffers.get());

    decrypted_.append(static_cast<char*>(buffers[1].pvBuffer),
                      buffers[1].cbBuffer);
    message_.erase(0, message_.size() - buffers[3].cbBuffer);

    if (SUCCEEDED(result)) {
      if (result == SEC_I_CONTEXT_EXPIRED) {
        status_ = Status::kClosing;
        result = context_.ApplyControlToken(SCHANNEL_SHUTDOWN);
        if (FAILED(result))
          return result;
      } else if (result == SEC_I_RENEGOTIATE) {
        status_ = Status::kNegotiate;
      }

      if (status_ != Status::kData)
        return Negotiate();
    } else if (result == SEC_E_INCOMPLETE_MESSAGE) {
      return S_OK;
    } else {
      message_.clear();
      return result;
    }
  }

  if (message_.size() > kBufferSize * 2)
    return S_FALSE;

  return S_OK;
}

HRESULT SecureChannel::Encrypt(const void* input, int length,
                               std::string* output) {
  if (input == nullptr && length != 0 || length < 0)
    return E_INVALIDARG;

  if (output == nullptr)
    return E_POINTER;

  auto cursor = static_cast<const char*>(input);
  auto memory = std::make_unique<char[]>(stream_sizes_.cbHeader +
                                         stream_sizes_.cbMaximumMessage +
                                         stream_sizes_.cbTrailer);
  SecurityBuffer buffers{
      {0, SECBUFFER_STREAM_HEADER, nullptr},
      {0, SECBUFFER_DATA, nullptr},
      {0, SECBUFFER_STREAM_TRAILER, nullptr},
  };

  for (DWORD remaining = length; remaining > 0;) {
    auto block_size = std::min(stream_sizes_.cbMaximumMessage, remaining);
    auto pointer = memory.get();

    buffers[0].cbBuffer = stream_sizes_.cbHeader;
    buffers[0].pvBuffer = pointer;
    pointer += stream_sizes_.cbHeader;

    buffers[1].cbBuffer = block_size;
    buffers[1].pvBuffer = pointer;
    memcpy(pointer, cursor, block_size);
    pointer += block_size;

    buffers[2].cbBuffer = stream_sizes_.cbTrailer;
    buffers[2].pvBuffer = pointer;

    auto result = context_.EncryptMessage(0, buffers.get());
    if (FAILED(result))
      return result;

    for (const auto& buffer : buffers)
      output->append(static_cast<char*>(buffer.pvBuffer), buffer.cbBuffer);

    remaining -= block_size;
    cursor += block_size;
  }

  return S_OK;
}

HRESULT SecureChannel::EnsureReading() {
  lock_.AssertAcquired();

  if (status_ == Status::kError || status_ == Status::kClosed)
    return S_FALSE;

  if (!base::AtomicRefCountIsZero(&reads_))
    return S_FALSE;

  auto buffer = std::make_unique<char[]>(kBufferSize);
  if (buffer == nullptr)
    return E_OUTOFMEMORY;

  auto result = channel_->ReadAsync(buffer.get(), kBufferSize, this);
  if (FAILED(result)) {
    status_ = Status::kError;
    LOG(ERROR) << "Failed to read: 0x" << std::hex << result;
    return result;
  }

  buffer.release();
  base::AtomicRefCountInc(&ref_count_);
  base::AtomicRefCountInc(&reads_);

  return S_OK;
}

HRESULT SecureChannel::WriteAsyncImpl(const std::string& message,
                                      Request* request) {
  auto buffer = std::make_unique<char[]>(message.size());
  if (buffer == nullptr)
    return E_OUTOFMEMORY;

  memcpy(buffer.get(), message.data(), message.size());

  return WriteAsyncImpl(std::move(buffer), message.size(), request);
}

HRESULT SecureChannel::WriteAsyncImpl(std::unique_ptr<char[]>&& buffer,
                                      size_t length, Request* request) {
  auto result =
      channel_->WriteAsync(buffer.get(), static_cast<int>(length), this);
  if (FAILED(result)) {
    status_ = Status::kError;
    LOG(ERROR) << "Failed to write: 0x" << std::hex << result;
    return result;
  }

  base::AtomicRefCountInc(&ref_count_);

  if (request != nullptr)
    writes_.insert(std::make_pair(buffer.get(), request));

  buffer.release();

  return S_OK;
}

void SecureChannel::OnRead(Channel* /*channel*/, HRESULT result,
                           void* raw_buffer, int length) {
  std::unique_ptr<char[]> buffer(static_cast<char*>(raw_buffer));

  base::AutoLock guard(lock_);

  if (SUCCEEDED(result) && length > 0) {
    message_.append(buffer.get(), length);

    result = CheckMessage();
    if (SUCCEEDED(result)) {
      if (status_ == Status::kNegotiate)
        result = Negotiate();
      else if (status_ == Status::kData)
        result = Decrypt();
      else
        result = E_FAIL;
    } else if (result == SEC_E_INCOMPLETE_MESSAGE) {
      result = S_OK;
    }

    if (SUCCEEDED(result) && result != S_FALSE) {
      result = channel_->ReadAsync(buffer.get(), kBufferSize, this);
      if (SUCCEEDED(result))
        buffer.release();
    }
  }

  if (FAILED(result) || length <= 0) {
    if (FAILED(result)) {
      status_ = Status::kError;
      LOG(ERROR) << "Failed to read: 0x" << std::hex << result;
    } else {
      status_ = Status::kClosed;
    }

    if (!base::AtomicRefCountDec(&ref_count_))
      deletable_.Broadcast();

    base::AtomicRefCountDec(&reads_);
  }

  if (read_work_ != nullptr)
    SubmitThreadpoolWork(read_work_);
}

void SecureChannel::OnWritten(Channel* /*channel*/, HRESULT result,
                              void* raw_buffer, int length) {
  std::unique_ptr<char[]> buffer(static_cast<char*>(raw_buffer));

  base::AutoLock guard(lock_);

  if (FAILED(result) || length <= 0) {
    if (FAILED(result)) {
      status_ = Status::kError;
      LOG(ERROR) << "Failed to write: 0x" << std::hex << result;
    } else {
      status_ = Status::kClosed;
    }
  }

  auto found = writes_.find(raw_buffer);
  if (found != writes_.end()) {
    auto request = std::move(found->second);
    writes_.erase(found);

    base::AutoUnlock unlock(lock_);

    if (length == 0)
      request->length = 0;

    request->listener->OnWritten(this, result, request->buffer,
                                 request->length);
  }

  if (!base::AtomicRefCountDec(&ref_count_))
    deletable_.Broadcast();
}

void SecureChannel::OnRead(PTP_CALLBACK_INSTANCE callback, void* instance,
                           PTP_WORK /*work*/) {
  CallbackMayRunLong(callback);
  static_cast<SecureChannel*>(instance)->OnRead();
}

void SecureChannel::OnRead() {
  while (true) {
    base::AutoLock guard(lock_);

    auto result = EnsureInitialized();
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to initialize session: 0x" << std::hex << result;
      status_ = Status::kError;
    }

    if (pending_reads_.empty() || status_ == Status::kNegotiate ||
        status_ == Status::kData && decrypted_.empty())
      break;

    auto request = std::move(pending_reads_.front());
    pending_reads_.pop();

    result = S_OK;
    if (!decrypted_.empty()) {
      auto size = std::min<size_t>(request->length, decrypted_.size());
      memcpy(request->buffer, decrypted_.data(), size);
      decrypted_.erase(0, size);
      request->length = static_cast<int>(size);
    } else if (status_ == Status::kError) {
      result = E_FAIL;
      request->length = -1;
    } else {
      request->length = 0;
    }

    base::AutoUnlock unlock(lock_);
    request->listener->OnRead(this, result, request->buffer, request->length);
  }

  base::AutoLock guard(lock_);
  auto result = EnsureReading();
  if (FAILED(result)) {
    status_ = Status::kError;
    LOG(ERROR) << "Failed to begin reading: 0x" << std::hex << result;
  }
}

void SecureChannel::OnWrite(PTP_CALLBACK_INSTANCE callback, void* instance,
                            PTP_WORK /*work*/) {
  CallbackMayRunLong(callback);
  static_cast<SecureChannel*>(instance)->OnWrite();
}

void SecureChannel::OnWrite() {
  while (true) {
    base::AutoLock guard(lock_);

    auto result = EnsureInitialized();
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to initialize session: 0x" << std::hex << result;
      status_ = Status::kError;
    }

    if (status_ == Status::kNegotiate || pending_writes_.empty())
      return;

    auto request = std::move(pending_writes_.front());
    pending_writes_.pop();

    result = E_UNEXPECTED;

    switch (status_) {
      case Status::kData: {
        std::string message;
        result = Encrypt(request->buffer, request->length, &message);
        if (SUCCEEDED(result)) {
          result = WriteAsyncImpl(message, request.get());
          if (SUCCEEDED(result)) {
            request.release();
            return;
          }
        }
        break;
      }

      case Status::kError:
        result = E_FAIL;
        request->length = -1;
        break;

      case Status::kClosing:
      case Status::kClosed:
        request->length = 0;
        break;

      default:
        LOG(FATAL) << "Invalid status: " << static_cast<int>(status_);
        break;
    }

    base::AutoUnlock unlock(lock_);
    request->listener->OnWritten(this, result, request->buffer,
                                 request->length);
  }
}

}  // namespace io
}  // namespace juno
