// Copyright (c) 2013 dacci.org

#include "net/scissors/scissors.h"

#include <madoka/concurrent/lock_guard.h>

#include "misc/registry_key.h"
#include "misc/schannel/schannel_credential.h"
#include "net/scissors/scissors_session.h"

Scissors::Scissors()
    : stopped_(), remote_port_(), remote_ssl_(), credential_() {
  empty_event_ = ::CreateEvent(NULL, TRUE, TRUE, NULL);
}

Scissors::~Scissors() {
  if (credential_ != NULL)
    delete credential_;

  ::CloseHandle(empty_event_);
}

bool Scissors::Setup(HKEY key) {
  RegistryKey reg_key(key);

  reg_key.QueryString("RemoteAddress", &remote_address_);
  reg_key.QueryInteger("RemotePort", &remote_port_);
  reg_key.QueryInteger("RemoteSSL", &remote_ssl_);

  reg_key.Detach();

  resolver_.ai_socktype = SOCK_STREAM;
  if (!resolver_.Resolve(remote_address_.c_str(), remote_port_))
    return false;

  if (remote_ssl_) {
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

void Scissors::EndSession(ScissorsSession* session) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  sessions_.remove(session);

  if (sessions_.empty())
    ::SetEvent(empty_event_);
}

bool Scissors::OnAccepted(AsyncSocket* client) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  if (stopped_)
    return false;

  ScissorsSession* session = new ScissorsSession(this);
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

void Scissors::OnReceivedFrom(AsyncDatagramSocket* socket, void* data,
                              int length, sockaddr* from, int from_length) {
}

void Scissors::OnError(DWORD error) {
}
