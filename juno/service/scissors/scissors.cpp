// Copyright (c) 2013 dacci.org

#include "service/scissors/scissors.h"

#include <madoka/concurrent/lock_guard.h>

#include "misc/registry_key.h"
#include "misc/security/schannel_credential.h"
#include "service/scissors/scissors_config.h"
#include "service/scissors/scissors_tcp_session.h"
#include "service/scissors/scissors_udp_session.h"

using madoka::net::AsyncSocket;

Scissors::Scissors(const std::shared_ptr<ServiceConfig>& config)
    : config_(std::static_pointer_cast<ScissorsConfig>(config)),
      stopped_(),
      credential_() {
}

Scissors::~Scissors() {
}

bool Scissors::Init() {
  if (config_->remote_ssl() && credential_ == nullptr) {
    credential_.reset(new SchannelCredential());
    if (credential_ == nullptr)
      return false;

    credential_->SetEnabledProtocols(SP_PROT_SSL3TLS1_X_CLIENTS);
    credential_->SetFlags(SCH_CRED_MANUAL_CRED_VALIDATION);

    SECURITY_STATUS status = credential_->AcquireHandle(SECPKG_CRED_OUTBOUND);
    if (FAILED(status))
      return false;
  }

  return true;
}

bool Scissors::UpdateConfig(const ServiceConfigPtr& config) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  config_ = std::static_pointer_cast<ScissorsConfig>(config);

  return Init();
}

void Scissors::Stop() {
  madoka::concurrent::LockGuard lock(&critical_section_);

  stopped_ = true;

  for (auto& session : sessions_)
    session->Stop();

  while (!sessions_.empty())
    empty_.Sleep(&critical_section_);
}

void Scissors::EndSession(Session* session) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  sessions_.remove(session);

  if (sessions_.empty())
    empty_.WakeAll();
}

bool Scissors::OnAccepted(const AsyncSocketPtr& client) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  if (stopped_)
    return false;

  ScissorsTcpSession* session = new ScissorsTcpSession(this);
  if (session == nullptr)
    return false;

  sessions_.push_back(session);

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
  if (session == nullptr)
    return false;

  sessions_.push_back(session);

  if (!session->Start()) {
    delete session;
    return false;
  }

  return true;
}

void Scissors::OnError(DWORD error) {
}
