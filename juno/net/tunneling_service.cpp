// Copyright (c) 2013 dacci.org

#include "net/tunneling_service.h"

#include <assert.h>

#include <madoka/concurrent/lock_guard.h>

using ::madoka::net::AsyncSocket;

TunnelingService* TunnelingService::instance_ = nullptr;

bool TunnelingService::Init() {
  Term();

  instance_ = new TunnelingService();
  return instance_ != nullptr;
}

void TunnelingService::Term() {
  if (instance_ != nullptr) {
    delete instance_;
    instance_ = nullptr;
  }
}

bool TunnelingService::Bind(const AsyncSocketPtr& a, const AsyncSocketPtr& b) {
  if (instance_ == nullptr)
    return false;

  return instance_->BindSocket(a, b) && instance_->BindSocket(b, a);
}

TunnelingService::TunnelingService() : stopped_() {
}

TunnelingService::~TunnelingService() {
  madoka::concurrent::LockGuard lock(&critical_section_);

  stopped_ = true;

  for (auto& session : sessions_)
    session->from_->Shutdown(SD_BOTH);

  while (!sessions_.empty())
    empty_.Sleep(&critical_section_);
}

bool TunnelingService::BindSocket(const AsyncSocketPtr& from,
                                  const AsyncSocketPtr& to) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  if (stopped_)
    return false;

  Session* pair = new Session(this, from, to);
  if (pair == nullptr)
    return false;

  bool started = pair->Start();
  if (started)
    sessions_.push_back(pair);
  else
    delete pair;

  return started;
}

void TunnelingService::EndSession(Session* session) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  sessions_.remove(session);
  if (sessions_.empty())
    empty_.WakeAll();

  delete session;
}

TunnelingService::Session::Session(TunnelingService* service,
                                   const AsyncSocketPtr& from,
                                   const AsyncSocketPtr& to)
    : service_(service), from_(from), to_(to), buffer_(new char[kBufferSize]) {
}

TunnelingService::Session::~Session() {
  from_->Shutdown(SD_BOTH);
  to_->Shutdown(SD_BOTH);
}

bool TunnelingService::Session::Start() {
  return from_->ReceiveAsync(buffer_.get(), kBufferSize, 0, this);
}

void TunnelingService::Session::OnReceived(AsyncSocket* socket, DWORD error,
                                           void* buffer, int length) {
  if (error != 0 || length == 0 ||
      !to_->SendAsync(buffer_.get(), length, 0, this))
    ::TrySubmitThreadpoolCallback(EndSession, this, nullptr);
}

void TunnelingService::Session::OnSent(AsyncSocket* socket, DWORD error,
                                       void* buffer, int length) {
  if (error != 0 || length == 0 ||
      !from_->ReceiveAsync(buffer_.get(), kBufferSize, 0, this))
    ::TrySubmitThreadpoolCallback(EndSession, this, nullptr);
}

void CALLBACK TunnelingService::Session::EndSession(
    PTP_CALLBACK_INSTANCE instance, void* param) {
  Session* session = static_cast<Session*>(param);
  session->service_->EndSession(session);
}
