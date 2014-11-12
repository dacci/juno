// Copyright (c) 2014 dacci.org

#include "net/socket_channel.h"

using ::madoka::net::AsyncSocket;

SocketChannel::SocketChannel(const AsyncSocketPtr& socket) : socket_(socket) {
}

SocketChannel::~SocketChannel() {
}

void SocketChannel::Close() {
  socket_->Shutdown(SD_BOTH);
  socket_->Close();
}

void SocketChannel::ReadAsync(void* buffer, int length, Listener* listener) {
  EventArgs* args = new EventArgs();
  if (args == nullptr) {
    listener->OnRead(this, E_OUTOFMEMORY, buffer, 0);
    return;
  }

  args->channel = this;
  args->listener = listener;

  socket_->ReceiveAsync(buffer, length, 0, args);
}

void SocketChannel::WriteAsync(const void* buffer, int length,
                              Listener* listener) {
  EventArgs* args = new EventArgs();
  if (args == nullptr) {
    listener->OnWritten(this, E_OUTOFMEMORY, const_cast<void*>(buffer), 0);
    return;
  }

  args->channel = this;
  args->listener = listener;

  socket_->SendAsync(buffer, length, 0, args);
}

void SocketChannel::EventArgs::OnReceived(AsyncSocket* socket, DWORD error,
                                         void* buffer, int length) {
  listener->OnRead(channel, error, buffer, length);
  delete this;
}

void SocketChannel::EventArgs::OnSent(AsyncSocket* socket, DWORD error,
                                     void* buffer, int length) {
  listener->OnWritten(channel, error, buffer, length);
  delete this;
}
