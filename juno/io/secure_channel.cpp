// Copyright (c) 2016 dacci.org

#include "io/secure_channel.h"

#include <base/logging.h>

#include <algorithm>
#include <string>
#include <vector>

namespace {

enum ContentType : uint8_t {
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

SecureChannel::SecureChannel(SchannelCredential* credential,
                             const std::shared_ptr<Channel>& channel,
                             bool inbound)
    : context_(credential),
      channel_(channel),
      inbound_(inbound),
      deletable_(&lock_),
      ref_count_(0),
      status_(kNegotiate),
      stream_sizes_(),
      reads_(0),
      read_work_(CreateThreadpoolWork(OnRead, this, nullptr)),
      write_work_(CreateThreadpoolWork(OnWrite, this, nullptr)) {
  HRESULT result;

  do {
    if (read_work_ == nullptr || write_work_ == nullptr) {
      result = E_HANDLE;
      break;
    }

    if (!inbound) {
      result = Negotiate();
      if (FAILED(result))
        break;
    }

    result = EnsureReading();
  } while (false);

  if (SUCCEEDED(result))
    return;

  status_ = kError;
  channel_->Close();
}

SecureChannel::~SecureChannel() {
  Close();

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
  auto result = S_OK;

  do {
    base::AutoLock guard(lock_);

    if (status_ == kError || status_ == kClosed)
      break;

    result = context_.ApplyControlToken(SCHANNEL_SHUTDOWN);
    if (FAILED(result))
      break;

    status_ = kClosing;
    message_.clear();

    result = Negotiate();
    if (FAILED(result))
      break;
  } while (false);

  if (SUCCEEDED(result))
    result = EnsureReading();

  if (FAILED(result)) {
    base::AutoLock guard(lock_);
    status_ = kError;
    channel_->Close();
  }
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

    pending_reads_.push_back(std::move(request));
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

    pending_writes_.push_back(std::move(request));
    if (pending_writes_.size() == 1)
      SubmitThreadpoolWork(write_work_);
  } catch (...) {
    return E_FAIL;
  }

  return S_OK;
}

HRESULT SecureChannel::CheckMessage() const {
  static const uint16_t kMaxLength = 16384;

  if (message_.size() < sizeof(TLS_RECORD))
    return SEC_E_INCOMPLETE_MESSAGE;

  auto record = reinterpret_cast<const TLS_RECORD*>(message_.data());

  if (_byteswap_ushort(record->length) > kMaxLength)
    return SEC_E_ILLEGAL_MESSAGE;

  switch (record->type) {
    case kChangeCipherSpec:
    case kAlert:
    case kHandshake:
      if (record->length == 0)
        return SEC_E_ILLEGAL_MESSAGE;
      break;

    case kApplicationData:
      break;

    default:
      return SEC_E_ILLEGAL_MESSAGE;
  }

  return S_OK;
}

HRESULT SecureChannel::Negotiate() {
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

    message_.erase(0, message_.size() - inputs[1].cbBuffer);

    for (auto& output : outputs) {
      if (output.pvBuffer == nullptr)
        continue;

      response.append(static_cast<char*>(output.pvBuffer), output.cbBuffer);
      FreeContextBuffer(output.pvBuffer);
    }

    if (result == SEC_E_INCOMPLETE_MESSAGE)
      break;

    if (result == SEC_E_OK) {
      if (status_ == kClosing)
        status_ = kClosed;
      else
        status_ = kData;
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

  if (status_ == kData)
    context_.QueryAttributes(SECPKG_ATTR_STREAM_SIZES, &stream_sizes_);

  return result;
}

HRESULT SecureChannel::Decrypt() {
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
        status_ = kClosing;
        result = context_.ApplyControlToken(SCHANNEL_SHUTDOWN);
        if (FAILED(result))
          return result;
      } else if (result == SEC_I_RENEGOTIATE) {
        status_ = kNegotiate;
      }

      if (status_ != kData)
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
  else
    return S_OK;
}

HRESULT SecureChannel::Encrypt(const void* input, int length,
                               std::string* output) {
  if (input == nullptr && length != 0 || length < 0)
    return E_INVALIDARG;

  if (output == nullptr)
    return E_POINTER;

  auto cursor = static_cast<const char*>(input);
  auto remaining = length;
  auto memory = std::make_unique<char[]>(stream_sizes_.cbHeader +
                                         stream_sizes_.cbMaximumMessage +
                                         stream_sizes_.cbTrailer);
  SecurityBuffer buffers{
      {0, SECBUFFER_STREAM_HEADER, nullptr},
      {0, SECBUFFER_DATA, nullptr},
      {0, SECBUFFER_STREAM_TRAILER, nullptr},
  };

  do {
    auto block_size =
        std::min<DWORD>(stream_sizes_.cbMaximumMessage, remaining);
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

    for (auto& buffer : buffers)
      output->append(static_cast<char*>(buffer.pvBuffer), buffer.cbBuffer);

    remaining -= block_size;
    cursor += block_size;
  } while (remaining > 0);

  return S_OK;
}

HRESULT SecureChannel::EnsureReading() {
  base::AutoLock guard(lock_);

  if (!base::AtomicRefCountIsZero(&reads_))
    return S_FALSE;

  auto buffer = std::make_unique<char[]>(kBufferSize);
  auto result = channel_->ReadAsync(buffer.get(), kBufferSize, this);
  if (SUCCEEDED(result)) {
    buffer.release();
    base::AtomicRefCountInc(&ref_count_);
    base::AtomicRefCountInc(&reads_);
  } else {
    status_ = kError;
    channel_->Close();
  }

  return result;
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
  if (SUCCEEDED(result)) {
    base::AtomicRefCountInc(&ref_count_);
    if (request != nullptr)
      writes_.insert(std::make_pair(buffer.get(), request));
    buffer.release();
  } else {
    channel_->Close();
    status_ = kError;
  }

  return result;
}

void SecureChannel::OnRead(Channel* /*channel*/, HRESULT result,
                           void* raw_buffer, int length) {
  std::unique_ptr<char[]> buffer(static_cast<char*>(raw_buffer));

  base::AutoLock guard(lock_);

  if (SUCCEEDED(result) && length > 0) {
    message_.append(buffer.get(), length);

    result = CheckMessage();
    if (SUCCEEDED(result)) {
      if (status_ == kNegotiate)
        result = Negotiate();
      else if (status_ == kData)
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
    if (FAILED(result))
      status_ = kError;
    else
      status_ = kClosed;

    channel_->Close();

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
    if (FAILED(result))
      status_ = kError;
    else
      status_ = kClosed;

    channel_->Close();
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
    lock_.Acquire();

    if (pending_reads_.empty() || status_ == kNegotiate ||
        status_ == kData && decrypted_.empty()) {
      lock_.Release();
      break;
    }

    auto request = std::move(pending_reads_.front());
    pending_reads_.pop_front();

    auto result = S_OK;
    if (!decrypted_.empty()) {
      auto size = std::min<size_t>(request->length, decrypted_.size());
      memcpy(request->buffer, decrypted_.data(), size);
      decrypted_.erase(0, size);
      request->length = static_cast<int>(size);
    } else if (status_ == kError) {
      result = E_FAIL;
      request->length = -1;
    } else {
      request->length = 0;
    }

    lock_.Release();

    request->listener->OnRead(this, result, request->buffer, request->length);
  }

  auto result = EnsureReading();
  if (FAILED(result)) {
    status_ = kError;
    channel_->Close();
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

    if (status_ == kNegotiate || pending_writes_.empty())
      return;

    auto request = std::move(pending_writes_.front());
    pending_writes_.pop_front();

    auto result = E_UNEXPECTED;

    switch (status_) {
      case kData: {
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

      case kError:
        result = E_FAIL;
        request->length = -1;
        break;

      case kClosing:
      case kClosed:
        request->length = 0;
        break;

      default:
        DCHECK(false) << "This must not occur.";
    }

    {
      base::AutoUnlock unlock(lock_);
      request->listener->OnWritten(this, result, request->buffer,
                                   request->length);
    }
  }
}
