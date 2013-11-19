// Copyright (c) 2013 dacci.org

#include "net/scissors/scissors_tcp_session.h"

#include <assert.h>

#include "misc/string_util.h"
#include "misc/schannel/schannel_context.h"
#include "net/tunneling_service.h"

#ifdef LEGACY_PLATFORM
  #define DELETE_THIS() \
    delete this
#else   // LEGACY_PLATFORM
  #define DELETE_THIS() \
    ::TrySubmitThreadpoolCallback(DeleteThis, this, NULL)
#endif  // LEGACY_PLATFORM

ScissorsTcpSession::ScissorsTcpSession(Scissors* service)
    : service_(service),
      client_(),
      remote_(),
      context_(),
      established_(),
      ref_count_(1),
      shutdown_() {
}

ScissorsTcpSession::~ScissorsTcpSession() {
  if (context_ != NULL) {
    delete context_;
    context_ = NULL;
  }

  if (client_buffer_ != NULL) {
    delete client_buffer_;
    client_buffer_ = NULL;
  }

  if (remote_buffer_ != NULL) {
    delete remote_buffer_;
    remote_buffer_ = NULL;
  }

  service_->EndSession(this);
}

bool ScissorsTcpSession::Start(AsyncSocket* client) {
  remote_.reset(new AsyncSocket());
  if (remote_ == NULL)
    return false;

  client_buffer_ = new char[kBufferSize];
  if (client_buffer_ == NULL)
    return false;

  remote_buffer_ = new char[kBufferSize];
  if (remote_buffer_ == NULL)
    return false;

  if (service_->remote_ssl_) {
    std::wstring target_name = ::to_wstring(service_->remote_address_);
    context_ = new SchannelContext(service_->credential_, target_name.c_str());
    if (context_ == NULL)
      return false;
  }

  client_.reset(client);

  if (!remote_->ConnectAsync(*service_->resolver_, this)) {
    client_ = NULL;
    return false;
  }

  return true;
}

void ScissorsTcpSession::Stop() {
  if (client_ != NULL)
    client_->Shutdown(SD_BOTH);
}

void ScissorsTcpSession::OnConnected(AsyncSocket* socket, DWORD error) {
  if (error != 0) {
    DELETE_THIS();
    return;
  }

  if (!service_->remote_ssl_) {
    if (TunnelingService::Bind(client_, remote_)) {
      client_ = NULL;
      remote_ = NULL;
    }

    DELETE_THIS();
    return;
  }

  if (!DoNegotiation()) {
    DELETE_THIS();
    return;
  }
}

void ScissorsTcpSession::OnReceived(AsyncSocket* socket, DWORD error,
                                    int length) {
  if (error == 0 && length > 0) {
    bool succeeded = false;

    if (socket == client_.get())
      succeeded = OnClientReceived(length);
    else if (socket == remote_.get())
      succeeded = OnRemoteReceived(length);
    else
      assert(false);

    if (succeeded)
      return;
  }

  EndSession(socket);
}

void ScissorsTcpSession::OnSent(AsyncSocket* socket, DWORD error, int length) {
  if (error == 0 && length > 0) {
    bool succeeded = false;

    if (socket == client_.get())
      succeeded = OnClientSent(length);
    else if (socket == remote_.get())
      succeeded = OnRemoteSent(length);
    else
      assert(false);

    if (succeeded)
      return;
  }

  EndSession(socket);
}

bool ScissorsTcpSession::SendAsync(const AsyncSocketPtr& socket,
                                   const SecBuffer& buffer) {
  return socket->SendAsync(buffer.pvBuffer, buffer.cbBuffer, 0, this);
}

bool ScissorsTcpSession::DoNegotiation() {
  negotiating_ = 1;

  token_input_.ClearBuffers();
  token_input_.AddBuffer(SECBUFFER_TOKEN, remote_data_.size(),
                         remote_data_.data());
  token_input_.AddBuffer(SECBUFFER_EMPTY);

  token_output_.ClearBuffers();
  token_output_.AddBuffer(SECBUFFER_TOKEN);
  token_output_.AddBuffer(SECBUFFER_ALERT);

  SECURITY_STATUS status = context_->InitializeContext(ISC_REQ_ALLOCATE_MEMORY,
                                                       &token_input_,
                                                       &token_output_);
  if (FAILED(status))
    return false;

  if (status == SEC_E_INCOMPLETE_MESSAGE)
    return remote_->ReceiveAsync(remote_buffer_, kBufferSize, 0, this);

  remote_data_.clear();

  switch (status) {
    case SEC_I_CONTINUE_NEEDED:
      if (token_output_[0].cbBuffer > 0)
        return SendAsync(remote_, token_output_[0]);
      else
        return remote_->ReceiveAsync(remote_buffer_, kBufferSize, 0, this);

    case SEC_E_OK:
      status = context_->QueryAttributes(SECPKG_ATTR_STREAM_SIZES,
                                         &stream_sizes_);
      if (FAILED(status))
        return false;

      if (token_output_[0].cbBuffer > 0) {
        negotiating_ = 2;
        return SendAsync(remote_, token_output_[0]);
      }

      return CompleteNegotiation();

    default:
      return false;
  }
}

bool ScissorsTcpSession::CompleteNegotiation() {
  negotiating_ = 0;

  if (established_) {
    if (!client_data_.empty())
      return DoEncryption();
  } else {
    established_ = true;

    if (!client_->ReceiveAsync(client_buffer_, kBufferSize, 0, this))
      return false;

    ref_count_ = 2;
  }

  if (shutdown_)
    return true;
  else
    return remote_->ReceiveAsync(remote_buffer_, kBufferSize, 0, this);
}

bool ScissorsTcpSession::DoEncryption() {
  char* buffer = new char[stream_sizes_.cbHeader + stream_sizes_.cbTrailer +
                          stream_sizes_.cbMaximumMessage];
  char* pointer = buffer;

  encrypted_.ClearBuffers();

  encrypted_.AddBuffer(SECBUFFER_STREAM_HEADER, stream_sizes_.cbHeader,
                       pointer);
  pointer += stream_sizes_.cbHeader;

  encrypted_.AddBuffer(SECBUFFER_DATA, client_data_.size(), pointer);
  ::memmove(pointer, client_data_.data(), client_data_.size());
  pointer += client_data_.size();

  encrypted_.AddBuffer(SECBUFFER_STREAM_TRAILER, stream_sizes_.cbTrailer,
                       pointer);
  encrypted_.AddBuffer(SECBUFFER_EMPTY);

  SECURITY_STATUS status = context_->EncryptMessage(0, &encrypted_);
  if (FAILED(status)) {
    delete[] buffer;
    return false;
  }

  int total_length = encrypted_[0].cbBuffer + encrypted_[1].cbBuffer +
                     encrypted_[2].cbBuffer;

  return remote_->SendAsync(buffer, total_length, 0, this);
}

bool ScissorsTcpSession::DoDecryption() {
  decrypted_.ClearBuffers();
  decrypted_.AddBuffer(SECBUFFER_DATA, remote_data_.size(),
                       remote_data_.data());
  decrypted_.AddBuffer(SECBUFFER_EMPTY);
  decrypted_.AddBuffer(SECBUFFER_EMPTY);
  decrypted_.AddBuffer(SECBUFFER_EMPTY);
  decrypted_.AddBuffer(SECBUFFER_TOKEN);

  SECURITY_STATUS status = context_->DecryptMessage(&decrypted_);

  switch (status) {
    case SEC_E_INCOMPLETE_MESSAGE:
      return remote_->ReceiveAsync(remote_buffer_, kBufferSize, 0, this);

    case SEC_I_RENEGOTIATE:
      return DoNegotiation();

    case SEC_E_OK:
      return SendAsync(client_, decrypted_[1]);

    case SEC_I_CONTEXT_EXPIRED:
      context_->ApplyControlToken(SCHANNEL_SHUTDOWN);
      return DoNegotiation();

    default:
      return false;
  }
}

void ScissorsTcpSession::EndSession(AsyncSocket* socket) {
  socket->Shutdown(SD_BOTH);

  switch (::InterlockedDecrement(&ref_count_)) {
    case 1:
      if (socket == client_.get()) {
        shutdown_ = true;
        context_->ApplyControlToken(SCHANNEL_SHUTDOWN);
        DoNegotiation();
      } else if (socket == remote_.get()) {
        client_->Shutdown(SD_BOTH);
      } else {
        assert(false);
      }
      break;

    case 0:
      DELETE_THIS();
      break;

    default:
      assert(false);
  }
}

bool ScissorsTcpSession::OnClientReceived(int length) {
  client_data_.insert(client_data_.end(), client_buffer_,
                      client_buffer_ + length);

  if (negotiating_)
    return true;
  else
    return DoEncryption();
}

bool ScissorsTcpSession::OnRemoteReceived(int length) {
  remote_data_.insert(remote_data_.end(), remote_buffer_,
                      remote_buffer_ + length);

  if (negotiating_)
    return DoNegotiation();
  else
    return DoDecryption();
}

bool ScissorsTcpSession::OnClientSent(int length) {
  ::memmove(remote_data_.data(), decrypted_[3].pvBuffer,
            decrypted_[3].cbBuffer);
  remote_data_.resize(decrypted_[3].cbBuffer);

  if (remote_data_.empty())
    return remote_->ReceiveAsync(remote_buffer_, kBufferSize, 0, this);
  else
    return DoDecryption();
}

bool ScissorsTcpSession::OnRemoteSent(int length) {
  if (negotiating_) {
    token_output_.FreeBuffers();
    token_output_.ClearBuffers();

    if (negotiating_ == 2)
      return CompleteNegotiation();
    else
      return remote_->ReceiveAsync(remote_buffer_, kBufferSize, 0, this);
  } else {
    if (encrypted_.cBuffers > 0) {
      delete[] encrypted_[0].pvBuffer;
      encrypted_.ClearBuffers();
      client_data_.clear();
    }

    return client_->ReceiveAsync(client_buffer_, kBufferSize, 0, this);
  }
}

void CALLBACK ScissorsTcpSession::DeleteThis(PTP_CALLBACK_INSTANCE instance,
                                             void* param) {
  delete static_cast<ScissorsTcpSession*>(param);
}
