// Copyright (c) 2013 dacci.org

#include "net/scissors/scissors_udp_session.h"

#include <assert.h>

#ifdef LEGACY_PLATFORM
  #define DELETE_THIS() \
    delete this
#else   // LEGACY_PLATFORM
  #define DELETE_THIS() \
    ::TrySubmitThreadpoolCallback(DeleteThis, this, NULL)
#endif  // LEGACY_PLATFORM

FILETIME ScissorsUdpSession::kTimerDueTime = {
  -kTimeout * 1000 * 10,   // in 100-nanoseconds
  -1
};

ScissorsUdpSession::ScissorsUdpSession(Scissors* service,
                                       Service::Datagram* datagram)
    : service_(service), datagram_(datagram), remote_(), buffer_(), timer_() {
#ifndef LEGACY_PLATFORM
  timer_ = ::CreateThreadpoolTimer(OnTimeout, this, NULL);
#endif  // LEGACY_PLATFORM
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

  if (timer_ != NULL) {
#ifdef LEGACY_PLATFORM
    ::DeleteTimerQueueTimer(NULL, timer_, INVALID_HANDLE_VALUE);
#else   // LEGACY_PLATFORM
    ::SetThreadpoolTimer(timer_, NULL, 0, 0);
    ::WaitForThreadpoolTimerCallbacks(timer_, TRUE);
    ::CloseThreadpoolTimer(timer_);
#endif  // LEGACY_PLATFORM
    timer_ = NULL;
  }

  service_->EndSession(this);
}

bool ScissorsUdpSession::Start() {
  remote_ = new AsyncDatagramSocket();
  if (remote_ == NULL)
    return false;

  buffer_.reset(new char[kBufferSize]);
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
#ifdef LEGACY_PLATFORM
  if (timer_ != NULL) {
    ::DeleteTimerQueueTimer(NULL, timer_, INVALID_HANDLE_VALUE);
    timer_ = NULL;
  }
#else   // LEGACY_PLATFORM
  assert(timer_ != NULL);
  ::SetThreadpoolTimer(timer_, NULL, 0, 0);
#endif  // LEGACY_PLATFORM

  if (error == 0) {
    addrinfo end_point = {};
    end_point.ai_addrlen = datagram_->from_length;
    end_point.ai_addr = datagram_->from;

    if (datagram_->socket->SendToAsync(buffer_.get(), length, 0, &end_point,
                                       this))
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
  if (error == 0) {
    do {
#ifdef LEGACY_PLATFORM
      assert(timer_ == NULL);
      if (!::CreateTimerQueueTimer(&timer_, NULL, OnTimeout, this, kTimeout, 0,
                                   WT_EXECUTEDEFAULT))
        break;
#else   // LEGACY_PLATFORM
      assert(timer_ != NULL);
      ::SetThreadpoolTimer(timer_, &kTimerDueTime, 0, 0);
#endif  // LEGACY_PLATFORM

      if (!remote_->ReceiveAsync(buffer_.get(), kBufferSize, 0, this))
        break;

      return;
    } while (false);
  }

  DELETE_THIS();
}

void ScissorsUdpSession::OnSentTo(AsyncDatagramSocket* socket, DWORD error,
                                  int length, sockaddr* to, int to_length) {
  DELETE_THIS();
}

#ifdef LEGACY_PLATFORM
void CALLBACK ScissorsUdpSession::OnTimeout(void* param, BOOLEAN fired) {
#else   // LEGACY_PLATFORM
void CALLBACK ScissorsUdpSession::OnTimeout(PTP_CALLBACK_INSTANCE instance,
                                            PVOID param, PTP_TIMER timer) {
#endif  // LEGACY_PLATFORM
  ScissorsUdpSession* session = static_cast<ScissorsUdpSession*>(param);
  session->Stop();
}

void CALLBACK ScissorsUdpSession::DeleteThis(PTP_CALLBACK_INSTANCE instance,
                                             void* param) {
  delete static_cast<ScissorsUdpSession*>(param);
}
