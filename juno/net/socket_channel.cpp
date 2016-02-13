// Copyright (c) 2014 dacci.org

#include "net/socket_channel.h"

using ::madoka::net::AsyncSocket;

class SocketChannel::Request : public AsyncSocket::Listener {
 public:
  Request(SocketChannel* channel, Channel::Listener* listener);

  void OnReceived(AsyncSocket* socket, HRESULT result, void* buffer,
                  int length, int flags) override;
  void OnSent(AsyncSocket* socket, HRESULT result, void* buffer,
              int length) override;

  void OnConnected(AsyncSocket* socket, HRESULT result,
                   const addrinfo* end_point) override {}
  void OnReceivedFrom(AsyncSocket* socket, HRESULT result, void* buffer,
                      int length, int flags, const sockaddr* address,
                      int address_length) override {}
  void OnSentTo(AsyncSocket* socket, HRESULT result, void* buffer, int length,
                const sockaddr* address, int address_length) override {}

  SocketChannel* const channel_;
  Channel::Listener* const listener_;
};

SocketChannel::SocketChannel(const AsyncSocketPtr& socket)
    : socket_(socket), closed_(), empty_(&lock_) {
}

SocketChannel::~SocketChannel() {
  Close();
  socket_->Close();

  base::AutoLock guard(lock_);
  while (!requests_.empty())
    empty_.Wait();
}

void SocketChannel::Close() {
  closed_ = true;

  if (socket_->connected())
    socket_->Shutdown(SD_BOTH);
  else
    socket_->Close();
}

void SocketChannel::ReadAsync(void* buffer, int length, Listener* listener) {
  if (listener == nullptr) {
    listener->OnRead(this, E_POINTER, buffer, 0);
    return;
  } else if (buffer == nullptr && length != 0 || length < 0) {
    listener->OnRead(this, E_INVALIDARG, buffer, 0);
    return;
  } else if (closed_ || socket_ == nullptr || !socket_->IsValid()) {
    listener->OnRead(this, E_HANDLE, buffer, 0);
    return;
  }

  auto request = std::make_unique<Request>(this, listener);
  if (request == nullptr) {
    listener->OnRead(this, E_OUTOFMEMORY, buffer, 0);
    return;
  }

  base::AutoLock guard(lock_);
  socket_->ReceiveAsync(buffer, length, 0, request.get());
  requests_.push_back(std::move(request));
}

void SocketChannel::WriteAsync(const void* buffer, int length,
                               Listener* listener) {
  if (listener == nullptr) {
    listener->OnWritten(this, E_POINTER, const_cast<void*>(buffer), 0);
    return;
  } else if (buffer == nullptr && length != 0 || length < 0) {
    listener->OnWritten(this, E_INVALIDARG, const_cast<void*>(buffer), 0);
    return;
  } else if (closed_ || socket_ == nullptr || !socket_->IsValid()) {
    listener->OnWritten(this, E_HANDLE, const_cast<void*>(buffer), 0);
    return;
  }

  auto request = std::make_unique<Request>(this, listener);
  if (request == nullptr) {
    listener->OnWritten(this, E_OUTOFMEMORY, const_cast<void*>(buffer), 0);
    return;
  }

  base::AutoLock guard(lock_);
  socket_->SendAsync(buffer, length, 0, request.get());
  requests_.push_back(std::move(request));
}

void SocketChannel::EndRequest(Request* request) {
  std::unique_ptr<Request> removed;
  base::AutoLock guard(lock_);

  for (auto i = requests_.begin(), l = requests_.end(); i != l; ++i) {
    if (i->get() == request) {
      removed = std::move(*i);
      requests_.erase(i);

      if (requests_.empty())
        empty_.Broadcast();

      break;
    }
  }
}

SocketChannel::Request::Request(SocketChannel* channel,
                                Channel::Listener* listener)
    : channel_(channel), listener_(listener) {
}

void SocketChannel::Request::OnReceived(AsyncSocket* socket, HRESULT result,
                                        void* buffer, int length, int flags) {
  listener_->OnRead(channel_, HRESULT_CODE(result), buffer, length);
  channel_->EndRequest(this);
}

void SocketChannel::Request::OnSent(AsyncSocket* socket, HRESULT result,
                                    void* buffer, int length) {
  listener_->OnWritten(channel_, HRESULT_CODE(result), buffer, length);
  channel_->EndRequest(this);
}
