// Copyright (c) 2013 dacci.org

#include "service/scissors/scissors.h"

#include <madoka/concurrent/lock_guard.h>
#include <madoka/net/resolver.h>

#include "misc/registry_key.h"
#include "misc/security/schannel_credential.h"
#include "net/datagram.h"
#include "net/datagram_channel.h"
#include "net/tunneling_service.h"
#include "service/scissors/scissors_config.h"
#include "service/scissors/scissors_tcp_session.h"
#include "service/scissors/scissors_udp_session.h"

using ::madoka::net::AsyncDatagramSocket;
using ::madoka::net::AsyncSocket;
using ::madoka::net::Resolver;

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
  if (!TrySubmitThreadpoolCallback(EndSessionImpl, session, nullptr))
    EndSessionImpl(session);
}

void CALLBACK Scissors::EndSessionImpl(PTP_CALLBACK_INSTANCE instance,
                                       void* param) {
  Session* session = static_cast<Session*>(param);
  session->service_->EndSessionImpl(session);
}

void Scissors::EndSessionImpl(Session* session) {
  std::unique_ptr<Session> removed;
  madoka::concurrent::LockGuard lock(&critical_section_);

  for (auto i = sessions_.begin(), l = sessions_.end(); i != l; ++i) {
    if (i->get() == session) {
      removed = std::move(*i);
      sessions_.erase(i);

      if (sessions_.empty())
        empty_.WakeAll();

      break;
    }
  }
}

void Scissors::OnAccepted(const ChannelPtr& client) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  if (stopped_)
    return;

  if (config_->remote_udp()) {
    Resolver resolver;
    resolver.SetType(SOCK_DGRAM);
    if (!resolver.Resolve(config_->remote_address().c_str(),
                          config_->remote_port()))
      return;

    auto socket = std::make_shared<AsyncDatagramSocket>();
    if (socket == nullptr)
      return;

    for (const addrinfo* end_point : resolver) {
      if (socket->Connect(end_point))
        break;

      socket->Close();
    }
    if (!socket->connected())
      return;

    auto remote = std::make_shared<DatagramChannel>(socket);
    if (remote == nullptr)
      return;

    if (!TunnelingService::Bind(client, remote)) {
      client->Close();
      remote->Close();
    }
  } else {
    auto session = std::make_unique<ScissorsTcpSession>(this, client);
    if (session == nullptr)
      return;

    if (session->Start())
      sessions_.push_back(std::move(session));
  }
}

void Scissors::OnReceivedFrom(const DatagramPtr& datagram) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  if (stopped_)
    return;

  std::unique_ptr<Session> session;

  if (config_->remote_udp()) {
    session.reset(new ScissorsUdpSession(this, datagram));
    if (session == nullptr)
      return;
  } else {
    return;
  }

  if (session->Start())
    sessions_.push_back(std::move(session));
}

void Scissors::OnError(DWORD error) {
}
