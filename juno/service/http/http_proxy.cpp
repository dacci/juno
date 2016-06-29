// Copyright (c) 2013 dacci.org

#include "service/http/http_proxy.h"

#include <string>
#include <utility>

#include "service/http/http_proxy_config.h"
#include "service/http/http_proxy_session.h"

HttpProxy::HttpProxy() : empty_(&lock_), stopped_() {}

HttpProxy::~HttpProxy() {
  Stop();
}

bool HttpProxy::UpdateConfig(const ServiceConfigPtr& config) {
  base::AutoLock guard(lock_);

  config_ = std::static_pointer_cast<HttpProxyConfig>(config);

  return true;
}

void HttpProxy::Stop() {
  base::AutoLock guard(lock_);

  stopped_ = true;

  for (auto& session : sessions_)
    session->Stop();

  while (!sessions_.empty())
    empty_.Wait();
}

void HttpProxy::EndSession(HttpProxySession* session) {
  auto pair = new ServiceSessionPair(this, session);
  if (pair == nullptr ||
      !TrySubmitThreadpoolCallback(EndSessionImpl, pair, nullptr)) {
    delete pair;
    EndSessionImpl(session);
  }
}

void HttpProxy::OnAccepted(const ChannelPtr& client) {
  auto session = std::make_unique<HttpProxySession>(this, config_, client);
  if (session == nullptr)
    return;

  base::AutoLock guard(lock_);

  if (stopped_)
    return;

  if (session->Start())
    sessions_.push_back(std::move(session));
}

void HttpProxy::OnReceivedFrom(const DatagramPtr& /*datagram*/) {
  // Do nothing
}

void CALLBACK HttpProxy::EndSessionImpl(PTP_CALLBACK_INSTANCE /*instance*/,
                                        void* param) {
  auto pair = static_cast<ServiceSessionPair*>(param);
  pair->first->EndSessionImpl(pair->second);
  delete pair;
}

void HttpProxy::EndSessionImpl(HttpProxySession* session) {
  std::unique_ptr<HttpProxySession> removed;
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
