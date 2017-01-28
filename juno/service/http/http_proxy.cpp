// Copyright (c) 2013 dacci.org

#include "service/http/http_proxy.h"

#include <windows.h>

#include <wincrypt.h>

#include <string>
#include <utility>

#include "service/http/http_proxy_session.h"
#include "service/http/http_util.h"

namespace juno {
namespace service {
namespace http {

HttpProxy::HttpProxy()
    : config_(nullptr),
      empty_(&lock_),
      stopped_(),
      auth_digest_(false),
      auth_basic_(false) {}

HttpProxy::~HttpProxy() {
  HttpProxy::Stop();
}

bool HttpProxy::UpdateConfig(const ServiceConfig* config) {
  base::AutoLock guard(lock_);

  config_ = static_cast<const HttpProxyConfig*>(config);
  SetCredential();

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

void HttpProxy::FilterHeaders(HttpHeaders* headers, bool request) const {
  for (auto& filter : config_->header_filters_) {
    if (request && filter.request || !request && filter.response) {
      switch (filter.action) {
        case HttpProxyConfig::FilterAction::kSet:
          headers->SetHeader(filter.name, filter.value);
          break;

        case HttpProxyConfig::FilterAction::kAppend:
          headers->AppendHeader(filter.name, filter.value);
          break;

        case HttpProxyConfig::FilterAction::kAdd:
          headers->AddHeader(filter.name, filter.value);
          break;

        case HttpProxyConfig::FilterAction::kUnset:
          headers->RemoveHeader(filter.name);
          break;

        case HttpProxyConfig::FilterAction::kMerge:
          headers->MergeHeader(filter.name, filter.value);
          break;

        case HttpProxyConfig::FilterAction::kEdit:
          headers->EditHeader(filter.name, filter.value, filter.replace, false);
          break;

        case HttpProxyConfig::FilterAction::kEditR:
          headers->EditHeader(filter.name, filter.value, filter.replace, true);
          break;
      }
    }
  }
}

void HttpProxy::ProcessAuthenticate(HttpResponse* response,
                                    HttpRequest* request) {
  base::AutoLock guard(lock_);

  DoProcessAuthenticate(response);
  DoProcessAuthorization(request);
}

void HttpProxy::ProcessAuthorization(HttpRequest* request) {
  base::AutoLock guard(lock_);

  DoProcessAuthorization(request);
}

void HttpProxy::OnAccepted(std::unique_ptr<io::Channel>&& client) {
  auto session =
      std::make_unique<HttpProxySession>(this, config_, std::move(client));
  if (session == nullptr)
    return;

  base::AutoLock guard(lock_);

  if (stopped_)
    return;

  if (session->Start())
    sessions_.push_back(std::move(session));
}

void HttpProxy::OnReceivedFrom(
    std::unique_ptr<io::net::Datagram>&& /*datagram*/) {
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

void HttpProxy::SetCredential() {
  digest_.SetCredential(config_->remote_proxy_user_,
                        config_->remote_proxy_password_);

  auto auth =
      config_->remote_proxy_user_ + ':' + config_->remote_proxy_password_;

  auto buffer_size = static_cast<DWORD>(((auth.size() - 1) / 3 + 1) * 4);
  basic_credential_.resize(buffer_size);
  buffer_size = static_cast<DWORD>(basic_credential_.capacity());

  CryptBinaryToStringA(reinterpret_cast<const BYTE*>(auth.c_str()),
                       static_cast<DWORD>(auth.size()),
                       CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                       &basic_credential_[0], &buffer_size);

  basic_credential_.insert(0, "Basic ");
}

void HttpProxy::DoProcessAuthenticate(HttpResponse* response) {
  if (!response->HeaderExists(kProxyAuthenticate))
    return;
  if (!config_->auth_remote_proxy_)
    return;

  auth_digest_ = false;
  auth_basic_ = false;

  for (auto& field : response->GetAllHeaders(kProxyAuthenticate)) {
    if (strncmp(field.c_str(), "Digest", 6) == 0)
      auth_digest_ = digest_.Input(field);
    else if (strncmp(field.c_str(), "Basic", 5) == 0)
      auth_basic_ = true;
  }
}

void HttpProxy::DoProcessAuthorization(HttpRequest* request) {
  // client's request has precedence
  if (request->HeaderExists(kProxyAuthorization))
    return;

  if (!config_->auth_remote_proxy_)
    return;

  if (auth_digest_) {
    std::string field;
    if (digest_.Output(request->method(), request->path(), &field))
      request->SetHeader(kProxyAuthorization, field);
  } else if (auth_basic_) {
    request->SetHeader(kProxyAuthorization, basic_credential_);
  }
}

}  // namespace http
}  // namespace service
}  // namespace juno
