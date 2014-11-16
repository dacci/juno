// Copyright (c) 2014 dacci.org

#include "net/socket_channel.h"

#include <madoka/net/async_socket.h>

using ::madoka::net::AsyncSocket;

SocketChannel::SocketChannel(const AsyncSocketPtr& socket) : socket_(socket) {
}

SocketChannel::~SocketChannel() {
  Close();
  socket_->Close();
}

void SocketChannel::Close() {
  socket_->Shutdown(SD_BOTH);
}

void SocketChannel::ReadAsync(void* buffer, int length, Listener* listener) {
  if (listener == nullptr) {
    listener->OnRead(this, E_POINTER, buffer, 0);
    return;
  } else if (buffer == nullptr && length != 0 || length < 0) {
    listener->OnRead(this, E_INVALIDARG, buffer, 0);
    return;
  } else if (socket_ == nullptr || !socket_->IsValid()) {
    listener->OnRead(this, E_HANDLE, buffer, 0);
    return;
  }

  Request* request = new Request(this, listener);
  if (request == nullptr) {
    listener->OnRead(this, E_OUTOFMEMORY, buffer, 0);
    return;
  }

  socket_->ReceiveAsync(buffer, length, 0, request);
}

void SocketChannel::WriteAsync(const void* buffer, int length,
                               Listener* listener) {
  if (listener == nullptr) {
    listener->OnWritten(this, E_POINTER, const_cast<void*>(buffer), 0);
    return;
  } else if (buffer == nullptr && length != 0 || length < 0) {
    listener->OnWritten(this, E_INVALIDARG, const_cast<void*>(buffer), 0);
    return;
  } else if (socket_ == nullptr || !socket_->IsValid()) {
    listener->OnWritten(this, E_HANDLE, const_cast<void*>(buffer), 0);
    return;
  }

  Request* request = new Request(this, listener);
  if (request == nullptr) {
    listener->OnWritten(this, E_OUTOFMEMORY, const_cast<void*>(buffer), 0);
    return;
  }

  socket_->SendAsync(buffer, length, 0, request);
}

SocketChannel::Request::Request(SocketChannel* channel, Listener* listener)
    : channel_(channel), listener_(listener) {
}

void SocketChannel::Request::OnReceived(AsyncSocket* socket, DWORD error,
                                        void* buffer, int length) {
  listener_->OnRead(channel_, error, buffer, length);
  delete this;
}

void SocketChannel::Request::OnSent(AsyncSocket* socket, DWORD error,
                                    void* buffer, int length) {
  listener_->OnWritten(channel_, error, buffer, length);
  delete this;
}
