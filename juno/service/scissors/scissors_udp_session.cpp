// Copyright (c) 2013 dacci.org

#include "service/scissors/scissors_udp_session.h"

#include <assert.h>

#include "service/scissors/scissors_config.h"

#define DELETE_THIS() \
  ::TrySubmitThreadpoolCallback(DeleteThis, this, nullptr)

using ::madoka::net::AsyncDatagramSocket;

FILETIME ScissorsUdpSession::kTimerDueTime = {
  -kTimeout * 1000 * 10,   // in 100-nanoseconds
  -1
};

ScissorsUdpSession::ScissorsUdpSession(Scissors* service,
                                       Service::Datagram* datagram)
    : service_(service), datagram_(datagram), remote_(), buffer_(), timer_() {
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

  service_->EndSession(this);
}

bool ScissorsUdpSession::Start() {
  if (!resolver_.Resolve(service_->config_->remote_address().c_str(),
                         service_->config_->remote_port()))
    return false;

  remote_.reset(new AsyncDatagramSocket());
  if (remote_ == nullptr)
    return false;

  buffer_.reset(new char[kBufferSize]);
  if (buffer_ == nullptr)
    return false;

  if (!remote_->Connect(*resolver_.begin()))
    return false;

  if (!remote_->SendAsync(datagram_->data, datagram_->data_length, 0, this))
    return false;

  return true;
}

void ScissorsUdpSession::Stop() {
  remote_->Close();
}

void ScissorsUdpSession::OnReceived(AsyncDatagramSocket* socket, DWORD error,
                                    void* buffer, int length) {
  assert(timer_ != nullptr);
  ::SetThreadpoolTimer(timer_, nullptr, 0, 0);

  if (error == 0 &&
      datagram_->socket->SendToAsync(buffer_.get(), length, 0,
                                     datagram_->from, datagram_->from_length,
                                     this))
    return;

  DELETE_THIS();
}

void ScissorsUdpSession::OnSent(AsyncDatagramSocket* socket, DWORD error,
                                void* buffer, int length) {
  if (error == 0) {
    do {
      assert(timer_ != nullptr);
      ::SetThreadpoolTimer(timer_, &kTimerDueTime, 0, 0);

      if (!remote_->ReceiveAsync(buffer_.get(), kBufferSize, 0, this))
        break;

      return;
    } while (false);
  }

  DELETE_THIS();
}

void ScissorsUdpSession::OnSentTo(AsyncDatagramSocket* socket, DWORD error,
                                  void* buffer, int length, sockaddr* to,
                                  int to_length) {
  DELETE_THIS();
}

void CALLBACK ScissorsUdpSession::OnTimeout(PTP_CALLBACK_INSTANCE instance,
                                            PVOID param, PTP_TIMER timer) {
  ScissorsUdpSession* session = static_cast<ScissorsUdpSession*>(param);
  session->Stop();
}

void CALLBACK ScissorsUdpSession::DeleteThis(PTP_CALLBACK_INSTANCE instance,
                                             void* param) {
  delete static_cast<ScissorsUdpSession*>(param);
}
