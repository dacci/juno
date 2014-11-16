// Copyright (c) 2014 dacci.org

#include "net/secure_socket_channel.h"

#include <stdint.h>
#include <stdlib.h>

#include <madoka/concurrent/lock_guard.h>
#include <madoka/net/socket.h>

#include <algorithm>
#include <string>

#ifdef min
#undef min
#endif

namespace {

enum RecordType : uint8_t {
  kChangeCipherSpec = 20,
  kAlert = 21,
  kHandshake = 22,
  kApplicationData = 23,
  kHeartbeat = 24,
};

#include <pshpack1.h>

typedef struct {
  RecordType type;
  uint8_t major_version;
  uint8_t minor_version;
  uint16_t length;
} SSL_RECORD;

#include <poppack.h>

}  // namespace

SecureSocketChannel::SecureSocketChannel(SchannelCredential* credential,
                                         const SocketPtr& socket, bool inbound)
    : context_(credential), socket_(socket), inbound_(inbound), closed_() {
  InitOnceInitialize(&init_once_);
}

SecureSocketChannel::~SecureSocketChannel() {
  Close();
  socket_->Close();
}

void SecureSocketChannel::Close() {
  if (!closed_) {
    closed_ = true;
    Shutdown();
  }
}

void SecureSocketChannel::ReadAsync(void* buffer, int length,
                                    Listener* listener) {
  if (listener == nullptr) {
    listener->OnRead(this, E_POINTER, buffer, 0);
    return;
  } else if (buffer == nullptr && length != 0 || length < 0) {
    listener->OnRead(this, E_INVALIDARG, buffer, 0);
    return;
  } else if (socket_ == nullptr) {
    listener->OnRead(this, E_HANDLE, buffer, 0);
    return;
  }

  {
    madoka::concurrent::LockGuard guard(&lock_);
    if (!decrypted_.empty()) {
      int size = std::min(static_cast<int>(decrypted_.size()), length);
      if (size > 0) {
        memmove(buffer, decrypted_.data(), size);
        decrypted_.erase(0, size);
      }
      listener->OnRead(this, 0, buffer, size);
      return;
    }
  }

  if (closed_) {
    listener->OnRead(this, E_HANDLE, buffer, 0);
    return;
  }

  ReadRequest* request = new ReadRequest(this, buffer, length, listener);
  if (request == nullptr) {
    listener->OnRead(this, E_OUTOFMEMORY, buffer, 0);
  } else if (!TrySubmitThreadpoolCallback(BeginRequest, request, nullptr)) {
    listener->OnRead(this, GetLastError(), buffer, 0);
    delete request;
  }
}

void SecureSocketChannel::WriteAsync(const void* buffer, int length,
                                     Listener* listener) {
  if (listener == nullptr) {
    listener->OnWritten(this, E_POINTER, const_cast<void*>(buffer), 0);
    return;
  } else if (buffer == nullptr && length != 0 || length < 0) {
    listener->OnWritten(this, E_INVALIDARG, const_cast<void*>(buffer), 0);
    return;
  } else if (socket_ == nullptr || !socket_->IsValid() || closed_) {
    listener->OnWritten(this, E_HANDLE, const_cast<void*>(buffer), 0);
    return;
  }

  WriteRequest* request = new WriteRequest(this, const_cast<void*>(buffer),
                                           length, listener);
  if (request == nullptr) {
    listener->OnWritten(this, E_OUTOFMEMORY, const_cast<void*>(buffer), 0);
  } else if (!TrySubmitThreadpoolCallback(BeginRequest, request, nullptr)) {
    listener->OnWritten(this, GetLastError(), const_cast<void*>(buffer), 0);
    delete request;
  }
}

void CALLBACK SecureSocketChannel::BeginRequest(PTP_CALLBACK_INSTANCE instance,
                                                void* param) {
  Request* request = static_cast<Request*>(param);
  request->Run();
  delete request;
}

BOOL CALLBACK SecureSocketChannel::InitOnceCallback(PINIT_ONCE init_once,
                                                    void* parameter,
                                                    void** context) {
  SecureSocketChannel* channel = static_cast<SecureSocketChannel*>(parameter);
  if (channel->inbound_)
    return channel->InitializeInbound(context);
  else
    return channel->InitializeOutbound(context);
}

BOOL SecureSocketChannel::InitializeInbound(void** /*context*/) {
  char buffer[4096];
  int length = socket_->Receive(buffer, sizeof(buffer), 0);
  if (length <= 0)
    return SEC_E_INTERNAL_ERROR;

  message_.append(buffer, length);

  return Negotiate() == SEC_E_OK;
}

BOOL SecureSocketChannel::InitializeOutbound(void** /*context*/) {
  return Negotiate() == SEC_E_OK;
}

HRESULT SecureSocketChannel::EnsureNegotiated() {
  if (!InitOnceExecuteOnce(&init_once_, InitOnceCallback, this, nullptr))
    return last_error_;

  if (!context_.IsValid())
    return last_error_;

  return SEC_E_OK;
}

HRESULT SecureSocketChannel::Negotiate() {
  madoka::concurrent::LockGuard guard(&lock_);

  while (true) {
    while (true) {
      SecBuffer inputs[] = {
        { 0, SECBUFFER_TOKEN, nullptr },
        { 0, SECBUFFER_EMPTY, nullptr }
      };
      SecBufferDesc input = { SECBUFFER_VERSION, _countof(inputs), inputs };
      if (!message_.empty()) {
        inputs[0].cbBuffer = message_.size();
        inputs[0].pvBuffer = &message_[0];
      }

      SecBuffer outputs[] = {
        { 0, SECBUFFER_TOKEN, nullptr },
        { 0, SECBUFFER_ALERT, nullptr }
      };
      SecBufferDesc output = { SECBUFFER_VERSION, _countof(outputs), outputs };

      if (inbound_)
        last_error_ = context_.AcceptContext(ASC_REQ_REPLAY_DETECT |
                                             ASC_REQ_SEQUENCE_DETECT |
                                             ASC_REQ_CONFIDENTIALITY |
                                             ASC_REQ_ALLOCATE_MEMORY |
                                             ASC_REQ_EXTENDED_ERROR |
                                             ASC_REQ_STREAM,
                                             &input,
                                             &output);
      else
        last_error_ = context_.InitializeContext(ISC_REQ_REPLAY_DETECT |
                                                 ISC_REQ_SEQUENCE_DETECT |
                                                 ISC_REQ_CONFIDENTIALITY |
                                                 ISC_REQ_ALLOCATE_MEMORY |
                                                 ISC_REQ_EXTENDED_ERROR |
                                                 ISC_REQ_STREAM,
                                                 &input,
                                                 &output);
      if (last_error_ == SEC_E_INCOMPLETE_MESSAGE)
        break;

      for (SecBuffer& buffer : outputs) {
        if (buffer.cbBuffer > 0)
          socket_->Send(buffer.pvBuffer, buffer.cbBuffer, 0);

        if (buffer.pvBuffer != nullptr)
          FreeContextBuffer(buffer.pvBuffer);
      }

      if (FAILED(last_error_))
        break;

      if (inputs[1].BufferType == SECBUFFER_EXTRA) {
        message_.erase(0, message_.size() - inputs[1].cbBuffer);
        continue;
      } else {
        message_.clear();
        break;
      }
    }
    if (last_error_ == SEC_E_OK) {
      last_error_ = context_.QueryAttributes(SECPKG_ATTR_STREAM_SIZES, &sizes_);
      break;
    } else if (last_error_ != SEC_E_INCOMPLETE_MESSAGE && FAILED(last_error_)) {
      break;
    }

    char buffer[4096];
    int length = socket_->Receive(buffer, sizeof(buffer), 0);
    if (length <= 0)
      return SEC_E_INTERNAL_ERROR;

    message_.append(buffer, length);
  }

  message_.clear();

  return last_error_;
}

HRESULT SecureSocketChannel::DecryptMessage(PSecBufferDesc message) {
  SSL_RECORD* record =
      reinterpret_cast<SSL_RECORD*>(message->pBuffers[0].pvBuffer);

  switch (record->type) {
    case kChangeCipherSpec:
    case kAlert:
    case kHandshake:
    case kApplicationData:
    case kHeartbeat:
      break;

    default:
      return SEC_E_ILLEGAL_MESSAGE;
  }

  if (_byteswap_ushort(record->length) > sizes_.cbMaximumMessage)
    return SEC_E_ILLEGAL_MESSAGE;

  return context_.DecryptMessage(message);
}

void SecureSocketChannel::Shutdown() {
  context_.ApplyControlToken(SCHANNEL_SHUTDOWN);
  message_.clear();
  Negotiate();
  socket_->Shutdown(SD_BOTH);
}

SecureSocketChannel::ReadRequest::ReadRequest(SecureSocketChannel* channel,
                                              void* buffer, int length,
                                              Listener* listener)
    : Request(channel, buffer, length, listener) {
}

void SecureSocketChannel::ReadRequest::Run() {
  if (channel_->EnsureNegotiated() != SEC_E_OK) {
    FireReadError(channel_->last_error_);
    return;
  }

  auto& message = channel_->message_;

  for (bool loop = true; loop;) {
    char buffer[4096];
    int length = channel_->socket_->Receive(buffer, sizeof(buffer), 0);
    if (length <= 0 && message.empty()) {
      FireReadError(WSAGetLastError());
      return;
    }

    message.append(buffer, length);

    while (!message.empty()) {
      SecBuffer inputs[] = {
        { message.size(), SECBUFFER_DATA, &message[0] },
        { 0, SECBUFFER_EMPTY, nullptr },
        { 0, SECBUFFER_EMPTY, nullptr },
        { 0, SECBUFFER_EMPTY, nullptr },
      };
      SecBufferDesc input = { SECBUFFER_VERSION, _countof(inputs), inputs };
      HRESULT status = channel_->DecryptMessage(&input);
      if (status == SEC_E_INCOMPLETE_MESSAGE)
        break;

      loop = false;

      if (FAILED(status)) {
        FireReadError(status);
        return;
      }

      if (inputs[1].cbBuffer > 0)
        channel_->decrypted_.append(
            static_cast<const char*>(inputs[1].pvBuffer), inputs[1].cbBuffer);

      message.erase(0, message.size() - inputs[3].cbBuffer);

      switch (status) {
        case SEC_I_CONTEXT_EXPIRED:
          channel_->Shutdown();
          break;

        case SEC_I_RENEGOTIATE:
          loop = true;
          channel_->Negotiate();
          break;
      }
    }
  }

  int size = std::min(static_cast<int>(channel_->decrypted_.size()), length_);
  if (size > 0) {
    memmove(buffer_, channel_->decrypted_.data(), size);
    channel_->decrypted_.erase(0, size);
  }
  listener_->OnRead(channel_, 0, buffer_, size);
}

SecureSocketChannel::WriteRequest::WriteRequest(SecureSocketChannel* channel,
                                                void* buffer, int length,
                                                Listener* listener)
    : Request(channel, buffer, length, listener) {
}

void SecureSocketChannel::WriteRequest::Run() {
  if (channel_->EnsureNegotiated() != SEC_E_OK) {
    FireWriteError(channel_->last_error_);
    return;
  }

  const char* input = static_cast<const char*>(buffer_);
  int remaining = length_;
  int sent = 0;

  std::unique_ptr<char[]> message(new char[channel_->sizes_.cbHeader +
                                           channel_->sizes_.cbTrailer +
                                           channel_->sizes_.cbMaximumMessage]);
  SecBuffer buffers[3];
  SecBufferDesc buffer = { SECBUFFER_VERSION, _countof(buffers), buffers };

  HRESULT status = SEC_E_OK;

  do {
    char* output = message.get();

    buffers[0].cbBuffer = channel_->sizes_.cbHeader;
    buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
    buffers[0].pvBuffer = output;
    output += buffers[0].cbBuffer;

    int size = std::min(
        remaining, static_cast<int>(channel_->sizes_.cbMaximumMessage));
    buffers[1].cbBuffer = size;
    buffers[1].BufferType = SECBUFFER_DATA;
    buffers[1].pvBuffer = output;

    memmove(output, input, size);
    output += size;
    input += size;
    remaining -= size;

    buffers[2].cbBuffer = channel_->sizes_.cbTrailer;
    buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
    buffers[2].pvBuffer = output;

    status = channel_->context_.EncryptMessage(0, &buffer);
    if (FAILED(status))
      break;

    int length = buffers[0].cbBuffer + buffers[1].cbBuffer +
                 buffers[2].cbBuffer;
    length = channel_->socket_->Send(message.get(), length, 0);
    if (length <= 0) {
      if (length < 0)
        status = WSAGetLastError();
      break;
    }

    sent += size;
  } while (remaining);

  listener_->OnWritten(channel_, status, buffer_, sent);
}
