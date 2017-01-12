// Copyright (c) 2013 dacci.org

#include "service/http/http_proxy.h"

#include <string>
#include <utility>

#include "service/http/http_proxy_config.h"
#include "service/http/http_proxy_session.h"

namespace juno {
namespace service {
namespace http {

HttpProxy::HttpProxy() : config_(nullptr), empty_(&lock_), stopped_() {}

HttpProxy::~HttpProxy() {
  HttpProxy::Stop();
}

bool HttpProxy::UpdateConfig(const ServiceConfig* config) {
  base::AutoLock guard(lock_);

  config_ = static_cast<const HttpProxyConfig*>(config);

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

void HttpProxy::ProcessAuthenticate(HttpResponse* response,
                                    HttpRequest* request) {
  base::AutoLock guard(lock_);

  auto config = const_cast<HttpProxyConfig*>(config_);
  config->DoProcessAuthenticate(response);
  config->DoProcessAuthorization(request);
}

void HttpProxy::ProcessAuthorization(HttpRequest* request) {
  base::AutoLock guard(lock_);

  auto config = const_cast<HttpProxyConfig*>(config_);
  config->DoProcessAuthorization(request);
}

void HttpProxy::OnAccepted(const io::ChannelPtr& client) {
  auto session = std::make_unique<HttpProxySession>(this, config_, client);
  if (session == nullptr)
    return;

  base::AutoLock guard(lock_);

  if (stopped_)
    return;

  if (session->Start())
    sessions_.push_back(std::move(session));
}

void HttpProxy::OnReceivedFrom(const io::net::DatagramPtr& /*datagram*/) {
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

  lock_.Acquire();

  for (auto i = sessions_.begin(), l = sessions_.end(); i != l; ++i) {
    if (i->get() == session) {
      removed = std::move(*i);
      sessions_.erase(i);

      if (sessions_.empty())
        empty_.Broadcast();

      break;
    }
  }

  lock_.Release();

  removed.reset();
}

}  // namespace http
}  // namespace service
}  // namespace juno
