// Copyright (c) 2013 dacci.org

#include "net/tunneling_service.h"

#include <assert.h>

#include <madoka/concurrent/lock_guard.h>

TunnelingService* TunnelingService::instance_ = NULL;

bool TunnelingService::Init() {
  Term();

  instance_ = new TunnelingService();
  return instance_ != NULL;
}

void TunnelingService::Term() {
  if (instance_ != NULL) {
    delete instance_;
    instance_ = NULL;
  }
}

bool TunnelingService::Bind(AsyncSocket* a, AsyncSocket* b) {
  if (instance_ == NULL)
    return false;

  return instance_->BindSocket(a, b) && instance_->BindSocket(b, a);
}

TunnelingService::TunnelingService() : stopped_() {
  empty_event_ = ::CreateEvent(NULL, TRUE, TRUE, NULL);
}

TunnelingService::~TunnelingService() {
  madoka::concurrent::LockGuard lock(&critical_section_);

  stopped_ = true;

  for (auto i = session_map_.begin(), l = session_map_.end(); i != l; ++i)
    i->first->Shutdown(SD_BOTH);

  while (true) {
    critical_section_.Unlock();
    ::WaitForSingleObject(empty_event_, INFINITE);
    critical_section_.Lock();

    if (session_map_.empty())
      break;
  }

  ::CloseHandle(empty_event_);
}

bool TunnelingService::BindSocket(AsyncSocket* from, AsyncSocket* to) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  if (stopped_)
    return false;

  Session* pair = new Session(this, from, to);
  if (pair == NULL)
    return false;

  bool started = pair->Start();
  if (started) {
    session_map_[from] = pair;
    ::ResetEvent(empty_event_);

    ++count_map_[from];
    ++count_map_[to];
  } else {
    delete pair;
  }

  return started;
}

void TunnelingService::EndSession(AsyncSocket* from, AsyncSocket* to) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  from->Shutdown(SD_BOTH);
  to->Shutdown(SD_BOTH);

  if (--count_map_[from] == 0) {
    count_map_.erase(from);
    delete from;
  }

  if (--count_map_[to] == 0) {
    count_map_.erase(to);
    delete to;
  }

  delete session_map_[from];
  session_map_.erase(from);
  if (session_map_.empty())
    ::SetEvent(empty_event_);
}

TunnelingService::Session::Session(TunnelingService* service, AsyncSocket* from,
                                   AsyncSocket* to)
    : service_(service), from_(from), to_(to), buffer_(new char[kBufferSize]) {
}

TunnelingService::Session::~Session() {
  delete buffer_;
}

bool TunnelingService::Session::Start() {
  return from_->ReceiveAsync(buffer_, kBufferSize, 0, this);
}

void TunnelingService::Session::OnConnected(AsyncSocket*, DWORD) {
  assert(false);
}

void TunnelingService::Session::OnReceived(AsyncSocket* socket, DWORD error,
                                           int length) {
  if (error != 0 || length == 0 ||
      !to_->SendAsync(buffer_, length, 0, this))
#ifdef LEGACY_PLATFORM
    service_->EndSession(from_, to_);
#else   // LEGACY_PLATFORM
    ::TrySubmitThreadpoolCallback(EndSession, this, NULL);
#endif  // LEGACY_PLATFORM
}

void TunnelingService::Session::OnSent(AsyncSocket* socket, DWORD error,
                                       int length) {
  if (error != 0 || length == 0 ||
      !from_->ReceiveAsync(buffer_, kBufferSize, 0, this))
#ifdef LEGACY_PLATFORM
    service_->EndSession(from_, to_);
#else   // LEGACY_PLATFORM
    ::TrySubmitThreadpoolCallback(EndSession, this, NULL);
#endif  // LEGACY_PLATFORM
}

void CALLBACK TunnelingService::Session::EndSession(
    PTP_CALLBACK_INSTANCE instance, void* param) {
  Session* session = static_cast<Session*>(param);
  session->service_->EndSession(session->from_, session->to_);
}
