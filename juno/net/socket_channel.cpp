// Copyright (c) 2014 dacci.org

#include "net/socket_channel.h"

#include <madoka/concurrent/lock_guard.h>
#include <madoka/net/socket_event_listener.h>

using ::madoka::net::AsyncSocket;

class SocketChannel::Request : public madoka::net::SocketEventAdapter {
 public:
  Request(SocketChannel* channel, Listener* listener);

  void OnReceived(AsyncSocket* socket, DWORD error, void* buffer,
                  int length) override;
  void OnSent(AsyncSocket* socket, DWORD error, void* buffer,
              int length) override;

  SocketChannel* const channel_;
  Listener* const listener_;
};

SocketChannel::SocketChannel(const AsyncSocketPtr& socket)
    : socket_(socket), closed_() {
}

SocketChannel::~SocketChannel() {
  Close();
  socket_->Close();

  madoka::concurrent::LockGuard guard(&lock_);
  while (!requests_.empty())
    empty_.Sleep(&lock_);
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

  madoka::concurrent::LockGuard guard(&lock_);
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

  madoka::concurrent::LockGuard guard(&lock_);
  socket_->SendAsync(buffer, length, 0, request.get());
  requests_.push_back(std::move(request));
}

void SocketChannel::EndRequest(Request* request) {
  std::unique_ptr<Request> removed;
  madoka::concurrent::LockGuard guard(&lock_);

  for (auto i = requests_.begin(), l = requests_.end(); i != l; ++i) {
    if (i->get() == request) {
      removed = std::move(*i);
      requests_.erase(i);

      if (requests_.empty())
        empty_.WakeAll();

      break;
    }
  }
}

SocketChannel::Request::Request(SocketChannel* channel, Listener* listener)
    : channel_(channel), listener_(listener) {
}

void SocketChannel::Request::OnReceived(AsyncSocket* socket, DWORD error,
                                        void* buffer, int length) {
  listener_->OnRead(channel_, error, buffer, length);
  channel_->EndRequest(this);
}

void SocketChannel::Request::OnSent(AsyncSocket* socket, DWORD error,
                                    void* buffer, int length) {
  listener_->OnWritten(channel_, error, buffer, length);
  channel_->EndRequest(this);
}
