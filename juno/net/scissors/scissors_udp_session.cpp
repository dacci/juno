// Copyright (c) 2013 dacci.org

#include "net/scissors/scissors_udp_session.h"

#ifdef LEGACY_PLATFORM
  #define DELETE_THIS() \
    delete this
#else   // LEGACY_PLATFORM
  #define DELETE_THIS() \
    ::TrySubmitThreadpoolCallback(DeleteThis, this, NULL)
#endif  // LEGACY_PLATFORM

ScissorsUdpSession::ScissorsUdpSession(Scissors* service,
                                       ServiceProvider::Datagram* datagram)
    : service_(service), datagram_(datagram), remote_(), buffer_() {
}

ScissorsUdpSession::~ScissorsUdpSession() {
  if (datagram_ != NULL) {
    delete[] reinterpret_cast<char*>(datagram_);
    datagram_ = NULL;
  }

  if (remote_ != NULL) {
    delete remote_;
    remote_ = NULL;
  }

  if (buffer_ != NULL) {
    delete[] buffer_;
    buffer_ = NULL;
  }

  service_->EndSession(this);
}

bool ScissorsUdpSession::Start() {
  remote_ = new AsyncDatagramSocket();
  if (remote_ == NULL)
    return false;

  buffer_ = new char[kBufferSize];
  if (buffer_ == NULL)
    return false;

  if (!remote_->Connect(*service_->resolver_))
    return false;

  if (!remote_->SendAsync(datagram_->data, datagram_->data_length, 0, this))
    return false;

  return true;
}

void ScissorsUdpSession::Stop() {
  remote_->Close();
}

void ScissorsUdpSession::OnReceived(AsyncDatagramSocket* socket, DWORD error,
                                    int length) {
  if (error == 0) {
    addrinfo end_point = {};
    end_point.ai_addrlen = datagram_->from_length;
    end_point.ai_addr = datagram_->from;

    if (datagram_->socket->SendToAsync(buffer_, length, 0, &end_point, this))
      return;
  }

  DELETE_THIS();
}

void ScissorsUdpSession::OnReceivedFrom(AsyncDatagramSocket* socket,
                                        DWORD error, int length, sockaddr* from,
                                        int from_length) {
}

void ScissorsUdpSession::OnSent(AsyncDatagramSocket* socket, DWORD error,
                                int length) {
  if (error == 0 && remote_->ReceiveAsync(buffer_, kBufferSize, 0, this))
    return;

  DELETE_THIS();
}

void ScissorsUdpSession::OnSentTo(AsyncDatagramSocket* socket, DWORD error,
                                  int length, sockaddr* to, int to_length) {
  DELETE_THIS();
}

void CALLBACK ScissorsUdpSession::DeleteThis(PTP_CALLBACK_INSTANCE instance,
                                             void* param) {
  delete static_cast<ScissorsUdpSession*>(param);
}
