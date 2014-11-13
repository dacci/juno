// Copyright (c) 2013 dacci.org

#include "service/scissors/scissors_udp_session.h"

#include <assert.h>

#include "service/scissors/scissors_config.h"

using ::madoka::net::AsyncDatagramSocket;

FILETIME ScissorsUdpSession::kTimerDueTime = {
  -kTimeout * 1000 * 10,   // in 100-nanoseconds
  -1
};

ScissorsUdpSession::ScissorsUdpSession(Scissors* service,
                                       Service::Datagram* datagram)
    : Session(service),
      datagram_(datagram),
      remote_(new AsyncDatagramSocket()),
      buffer_(new char[kBufferSize]),
      timer_() {
  resolver_.SetType(SOCK_DGRAM);
  timer_ = ::CreateThreadpoolTimer(OnTimeout, this, nullptr);
}

ScissorsUdpSession::~ScissorsUdpSession() {
  if (datagram_ != nullptr) {
    delete[] reinterpret_cast<char*>(datagram_);
    datagram_ = nullptr;
  }

  if (timer_ != nullptr) {
    ::SetThreadpoolTimer(timer_, nullptr, 0, 0);
    ::WaitForThreadpoolTimerCallbacks(timer_, TRUE);
    ::CloseThreadpoolTimer(timer_);
    timer_ = nullptr;
  }
}

bool ScissorsUdpSession::Start() {
  if (remote_ == nullptr)
    return false;

  if (buffer_ == nullptr)
    return false;

  if (!resolver_.Resolve(service_->config_->remote_address().c_str(),
                         service_->config_->remote_port()))
    return false;

  if (!remote_->Connect(*resolver_.begin()))
    return false;

  remote_->SendAsync(datagram_->data, datagram_->data_length, 0, this);

  return true;
}

void ScissorsUdpSession::Stop() {
  remote_->Close();
}

void ScissorsUdpSession::OnReceived(AsyncDatagramSocket* socket, DWORD error,
                                    void* buffer, int length) {
  assert(timer_ != nullptr);
  ::SetThreadpoolTimer(timer_, nullptr, 0, 0);

  if (error == 0)
    datagram_->socket->SendToAsync(buffer_.get(), length, 0, datagram_->from,
                                   datagram_->from_length, this);
  else
    service_->EndSession(this);
}

void ScissorsUdpSession::OnSent(AsyncDatagramSocket* socket, DWORD error,
                                void* buffer, int length) {
  if (error != 0) {
    service_->EndSession(this);
    return;
  }

  assert(timer_ != nullptr);
  ::SetThreadpoolTimer(timer_, &kTimerDueTime, 0, 0);
  remote_->ReceiveAsync(buffer_.get(), kBufferSize, 0, this);
}

void ScissorsUdpSession::OnSentTo(AsyncDatagramSocket* socket, DWORD error,
                                  void* buffer, int length, sockaddr* to,
                                  int to_length) {
  service_->EndSession(this);
}

void CALLBACK ScissorsUdpSession::OnTimeout(PTP_CALLBACK_INSTANCE instance,
                                            PVOID param, PTP_TIMER timer) {
  ScissorsUdpSession* session = static_cast<ScissorsUdpSession*>(param);
  session->Stop();
}
