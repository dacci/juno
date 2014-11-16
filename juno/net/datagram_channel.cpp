// Copyright (c) 2014 dacci.org

#include "net/datagram_channel.h"

#include <madoka/net/async_datagram_socket.h>

using ::madoka::net::AsyncDatagramSocket;

DatagramChannel::DatagramChannel(const AsyncDatagramSocketPtr& socket)
    : socket_(socket) {
}

DatagramChannel::~DatagramChannel() {
  Close();
}

void DatagramChannel::Close() {
  socket_->Close();
}

void DatagramChannel::ReadAsync(void* buffer, int length, Listener* listener) {
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

void DatagramChannel::WriteAsync(const void* buffer, int length,
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

DatagramChannel::Request::Request(DatagramChannel* channel, Listener* listener)
    : channel_(channel), listener_(listener) {
}

void DatagramChannel::Request::OnReceived(AsyncDatagramSocket* socket,
                                          DWORD error, void* buffer,
                                          int length) {
  listener_->OnRead(channel_, error, buffer, length);
  delete this;
}

void DatagramChannel::Request::OnSent(AsyncDatagramSocket* socket, DWORD error,
                                      void* buffer, int length) {
  listener_->OnWritten(channel_, error, buffer, length);
  delete this;
}
