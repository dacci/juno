// Copyright (c) 2013 dacci.org

#include "service/socks/socks_proxy.h"

#include "service/socks/socks_proxy_session.h"

using ::madoka::net::AsyncSocket;

SocksProxy::SocksProxy() : empty_(&lock_), stopped_() {
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
  base::AutoLock guard(lock_);

  stopped_ = true;

  for (auto& session : sessions_)
    session->Stop();

  while (!sessions_.empty())
    empty_.Wait();
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
  base::AutoLock guard(lock_);

  if (stopped_)
    return;

  auto session = std::make_unique<SocksProxySession>(this, client);
  if (session == nullptr)
    return;

  if (session->Start())
    sessions_.push_back(std::move(session));
}

void SocksProxy::OnReceivedFrom(const DatagramPtr& datagram) {
  // Do nothing
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
  std::unique_ptr<SocksProxySession> removed;
  base::AutoLock guard(lock_);

  for (auto i = sessions_.begin(), l = sessions_.end(); i != l; ++i) {
    if (i->get() == session) {
      removed = std::move(*i);
      sessions_.erase(i);

      if (sessions_.empty())
        empty_.Broadcast();

      break;
    }
  }
}
