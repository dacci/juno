// Copyright (c) 2013 dacci.org

#include "net/http/http_proxy.h"

#include <madoka/concurrent/lock_guard.h>

#include <utility>

#include "misc/registry_key-inl.h"
#include "net/http/http_proxy_config.h"
#include "net/http/http_proxy_session.h"

const std::string HttpProxy::kProxyAuthenticate("Proxy-Authenticate");
const std::string HttpProxy::kProxyAuthorization("Proxy-Authorization");

HttpProxy::HttpProxy(HttpProxyConfig* config)
    : config_(config),
      stopped_(),
      auth_digest_(),
      auth_basic_(),
      digest_(config_->remote_proxy_user(), config_->remote_proxy_password()) {
}

HttpProxy::~HttpProxy() {
  Stop();
}

bool HttpProxy::Init() {
  return true;
}

void HttpProxy::Stop() {
  madoka::concurrent::LockGuard lock(&critical_section_);

  stopped_ = true;

  for (auto i = sessions_.begin(), l = sessions_.end(); i != l; ++i)
    (*i)->Stop();

  while (!sessions_.empty())
    empty_.Sleep(&critical_section_);
}

void HttpProxy::FilterHeaders(HttpHeaders* headers, bool request) {
  madoka::concurrent::ReadLock read_lock(&config_->lock_);
  madoka::concurrent::LockGuard guard(&read_lock);

  for (auto i = config_->header_filters_.begin(),
            l = config_->header_filters_.end(); i != l; ++i) {
    auto& filter = *i;

    if (request && filter.request || !request && filter.response) {
      switch (filter.action) {
        case HttpProxyConfig::Set:
          headers->SetHeader(filter.name, filter.value);
          break;

        case HttpProxyConfig::Append:
          headers->AppendHeader(filter.name, filter.value);
          break;

        case HttpProxyConfig::Add:
          headers->AddHeader(filter.name, filter.value);
          break;

        case HttpProxyConfig::Unset:
          headers->RemoveHeader(filter.name);
          break;

        case HttpProxyConfig::Merge:
          headers->MergeHeader(filter.name, filter.value);
          break;

        case HttpProxyConfig::Edit:
          headers->EditHeader(filter.name, filter.value, filter.replace, false);
          break;

        case HttpProxyConfig::EditR:
          headers->EditHeader(filter.name, filter.value, filter.replace, true);
          break;
      }
    }
  }
}

void HttpProxy::ProcessAuthenticate(HttpResponse* response) {
  if (!config_->auth_remote_proxy())
    return;
  if (!response->HeaderExists(kProxyAuthenticate))
    return;

  madoka::concurrent::LockGuard lock(&critical_section_);

  auth_digest_ = false;
  auth_basic_ = false;

  for (auto& field : response->GetAllHeaders(kProxyAuthenticate)) {
    if (::strncmp(field.c_str(), "Digest", 6) == 0)
      auth_digest_ = digest_.Input(field);
    else if (::strncmp(field.c_str(), "Basic", 5) == 0)
      auth_basic_ = true;
  }
}

void HttpProxy::ProcessAuthorization(HttpRequest* request) {
  if (!config_->auth_remote_proxy())
    return;

  // client's request has precedence
  if (request->HeaderExists(kProxyAuthorization))
    return;

  if (auth_digest_) {
    madoka::concurrent::LockGuard lock(&critical_section_);

    std::string field;
    if (digest_.Output(request->method(), request->path(), &field))
      request->SetHeader(kProxyAuthorization, field);
  } else if (auth_basic_) {
    request->SetHeader(kProxyAuthorization, config_->basic_authorization());
  }
}

void HttpProxy::EndSession(HttpProxySession* session) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  sessions_.remove(session);
  if (sessions_.empty())
    empty_.WakeAll();
}

bool HttpProxy::OnAccepted(madoka::net::AsyncSocket* client) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  if (stopped_)
    return false;

  HttpProxySession* session = new HttpProxySession(this, config_);
  if (session == NULL)
    return false;

  sessions_.push_back(session);

  if (!session->Start(client)) {
    delete session;
    return false;
  }

  return true;
}

bool HttpProxy::OnReceivedFrom(Datagram* datagram) {
  return false;
}

void HttpProxy::OnError(DWORD error) {
}

void HttpProxy::set_config(HttpProxyConfig* config) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  *config_ = *config;
  digest_.SetCredential(config_->remote_proxy_user(),
                        config_->remote_proxy_password());
}
