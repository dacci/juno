// Copyright (c) 2013 dacci.org

#include "net/tunneling_service.h"

#include <assert.h>

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
  ::InitializeCriticalSection(&critical_section_);
}

TunnelingService::~TunnelingService() {
  stopped_ = true;

  ::EnterCriticalSection(&critical_section_);
  for (auto i = session_map_.begin(), l = session_map_.end(); i != l; ++i)
    i->first->Shutdown(SD_BOTH);
  ::LeaveCriticalSection(&critical_section_);

  ::WaitForSingleObject(empty_event_, INFINITE);

  ::CloseHandle(empty_event_);
  ::DeleteCriticalSection(&critical_section_);
}

bool TunnelingService::BindSocket(AsyncSocket* from, AsyncSocket* to) {
  if (stopped_)
    return false;

  Session* pair = new Session(this, from, to);
  if (pair == NULL)
    return false;

  ::EnterCriticalSection(&critical_section_);

  bool started = pair->Start();
  if (started) {
    session_map_[from] = pair;
    ::ResetEvent(empty_event_);

    ++count_map_[from];
    ++count_map_[to];
  } else {
    delete pair;
  }

  ::LeaveCriticalSection(&critical_section_);

  return started;
}

void TunnelingService::EndSession(AsyncSocket* from, AsyncSocket* to) {
  ::EnterCriticalSection(&critical_section_);

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

  ::LeaveCriticalSection(&critical_section_);
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
  if (error != 0 || length == 0) {
    service_->EndSession(from_, to_);
    return;
  }

  to_->SendAsync(buffer_, length, 0, this);
}

void TunnelingService::Session::OnSent(AsyncSocket* socket, DWORD error,
                                       int length) {
  if (error != 0 || length == 0) {
    service_->EndSession(from_, to_);
    return;
  }

  from_->ReceiveAsync(buffer_, kBufferSize, 0, this);
}
