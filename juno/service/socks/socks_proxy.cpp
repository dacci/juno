// Copyright (c) 2013 dacci.org

#include "service/socks/socks_proxy.h"

#include <madoka/concurrent/lock_guard.h>

#include "service/socks/socks_proxy_session.h"

using ::madoka::net::AsyncSocket;

SocksProxy::SocksProxy() : stopped_() {
}

SocksProxy::~SocksProxy() {
}

bool SocksProxy::Setup(HKEY key) {
  return true;
}

bool SocksProxy::UpdateConfig(const ServiceConfigPtr& config) {
  return true;
}

void SocksProxy::Stop() {
  madoka::concurrent::LockGuard lock(&critical_section_);

  stopped_ = true;

  for (auto& session : sessions_)
    session->Stop();

  while (!sessions_.empty())
    empty_.Sleep(&critical_section_);
}

void SocksProxy::EndSession(SocksProxySession* session) {
  auto pair = new ServiceSessionPair(this, session);
  if (pair == nullptr ||
      !TrySubmitThreadpoolCallback(EndSessionImpl, pair, nullptr)) {
    delete pair;
    EndSessionImpl(session);
  }
}

void SocksProxy::OnAccepted(const ChannelPtr& client) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  if (stopped_)
    return;

  std::unique_ptr<SocksProxySession> session(
      new SocksProxySession(this, client));
  if (session == nullptr)
    return;

  if (session->Start())
    sessions_.push_back(std::move(session));
}

bool SocksProxy::OnReceivedFrom(Datagram* datagram) {
  return false;
}

void SocksProxy::OnError(DWORD error) {
}

void CALLBACK SocksProxy::EndSessionImpl(PTP_CALLBACK_INSTANCE instance,
                                         void* param) {
  auto pair = static_cast<ServiceSessionPair*>(param);
  pair->first->EndSessionImpl(pair->second);
  delete pair;
}

void SocksProxy::EndSessionImpl(SocksProxySession* session) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  for (auto i = sessions_.begin(), l = sessions_.end(); i != l; ++i) {
    if (i->get() == session) {
      sessions_.erase(i);
      break;
    }
  }

  if (sessions_.empty())
    empty_.WakeAll();
}
