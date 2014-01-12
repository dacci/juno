// Copyright (c) 2013 dacci.org

#include "net/scissors/scissors.h"

#include <madoka/concurrent/lock_guard.h>

#include "misc/registry_key.h"
#include "misc/schannel/schannel_credential.h"
#include "net/scissors/scissors_config.h"
#include "net/scissors/scissors_tcp_session.h"
#include "net/scissors/scissors_udp_session.h"

Scissors::Scissors(ScissorsConfig* config)
    : config_(config), stopped_(), credential_() {
  empty_event_ = ::CreateEvent(NULL, TRUE, TRUE, NULL);
}

Scissors::~Scissors() {
  if (credential_ != NULL)
    delete credential_;

  ::CloseHandle(empty_event_);
}

bool Scissors::Init() {
  if (config_->remote_udp())
    resolver_.ai_socktype = SOCK_DGRAM;
  else
    resolver_.ai_socktype = SOCK_STREAM;

  // XXX(dacci): saving resolved name could be harmful.
  if (!resolver_.Resolve(config_->remote_address().c_str(),
                         config_->remote_port()))
    return false;

  if (config_->remote_ssl()) {
    credential_ = new SchannelCredential();
    if (credential_ == NULL)
      return false;

    credential_->SetEnabledProtocols(SP_PROT_SSL3TLS1_X_CLIENTS);
    credential_->SetFlags(SCH_CRED_MANUAL_CRED_VALIDATION);

    SECURITY_STATUS status = credential_->AcquireHandle(SECPKG_CRED_OUTBOUND);
    if (FAILED(status))
      return false;
  }

  return true;
}

void Scissors::Stop() {
  madoka::concurrent::LockGuard lock(&critical_section_);

  stopped_ = true;

  for (auto i = sessions_.begin(), l = sessions_.end(); i != l; ++i)
    (*i)->Stop();

  while (true) {
    critical_section_.Unlock();
    ::WaitForSingleObject(empty_event_, INFINITE);
    critical_section_.Lock();

    if (sessions_.empty())
      break;
  }
}

void Scissors::EndSession(Session* session) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  sessions_.remove(session);

  if (sessions_.empty())
    ::SetEvent(empty_event_);
}

bool Scissors::OnAccepted(AsyncSocket* client) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  if (stopped_)
    return false;

  ScissorsTcpSession* session = new ScissorsTcpSession(this);
  if (session == NULL)
    return false;

  sessions_.push_back(session);
  ::ResetEvent(empty_event_);

  if (!session->Start(client)) {
    delete session;
    return false;
  }

  return true;
}

bool Scissors::OnReceivedFrom(Datagram* datagram) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  if (stopped_)
    return false;

  ScissorsUdpSession* session = new ScissorsUdpSession(this, datagram);
  if (session == NULL)
    return false;

  sessions_.push_back(session);
  ::ResetEvent(empty_event_);

  if (!session->Start()) {
    delete session;
    return false;
  }

  return true;
}

void Scissors::OnError(DWORD error) {
}
