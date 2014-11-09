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
  madoka::concurrent::LockGuard lock(&critical_section_);

  sessions_.remove(session);

  if (sessions_.empty())
    empty_.WakeAll();
}

bool SocksProxy::OnAccepted(const AsyncSocketPtr& client) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  if (stopped_)
    return false;

  SocksProxySession* session = new SocksProxySession(this);
  if (session == nullptr)
    return false;

  sessions_.push_back(session);

  if (!session->Start(client)) {
    delete session;
    return false;
  }

  return true;
}

bool SocksProxy::OnReceivedFrom(Datagram* datagram) {
  return false;
}

void SocksProxy::OnError(DWORD error) {
}
