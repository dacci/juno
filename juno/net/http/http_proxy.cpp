// Copyright (c) 2013 dacci.org

#include "net/http/http_proxy.h"

#include <wincrypt.h>

#include <madoka/concurrent/lock_guard.h>

#include <utility>

#include "misc/registry_key-inl.h"
#include "net/http/http_proxy_config.h"
#include "net/http/http_proxy_session.h"

const std::string HttpProxy::kProxyAuthenticate("Proxy-Authenticate");
const std::string HttpProxy::kProxyAuthorization("Proxy-Authorization");

HttpProxy::HttpProxy(const ServiceConfigPtr& config)
    : config_(std::static_pointer_cast<HttpProxyConfig>(config)),
      stopped_(),
      auth_digest_(),
      auth_basic_(),
      digest_(config_->remote_proxy_user(), config_->remote_proxy_password()) {
  SetBasicCredential();
}

HttpProxy::~HttpProxy() {
  Stop();
}

bool HttpProxy::Init() {
  return true;
}

bool HttpProxy::UpdateConfig(const ServiceConfigPtr& config) {
  madoka::concurrent::WriteLock write_lock(&lock_);
  madoka::concurrent::LockGuard guard(&write_lock);

  config_ = std::static_pointer_cast<HttpProxyConfig>(config);

  digest_.SetCredential(config_->remote_proxy_user(),
                        config_->remote_proxy_password());
  SetBasicCredential();

  return true;
}

void HttpProxy::Stop() {
  madoka::concurrent::WriteLock write_lock(&lock_);
  madoka::concurrent::LockGuard guard(&write_lock);

  stopped_ = true;

  for (auto& session : sessions_)
    session->Stop();

  while (!sessions_.empty())
    empty_.Sleep(&lock_, false);
}

void HttpProxy::FilterHeaders(HttpHeaders* headers, bool request) {
  madoka::concurrent::ReadLock read_lock(&lock_);
  madoka::concurrent::LockGuard guard(&read_lock);

  for (auto& filter : config_->header_filters()) {
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

  madoka::concurrent::WriteLock write_lock(&lock_);
  madoka::concurrent::LockGuard guard(&write_lock);

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
    madoka::concurrent::ReadLock read_lock(&lock_);
    madoka::concurrent::LockGuard guard(&read_lock);

    std::string field;
    if (digest_.Output(request->method(), request->path(), &field))
      request->SetHeader(kProxyAuthorization, field);
  } else if (auth_basic_) {
    request->SetHeader(kProxyAuthorization, basic_credential_);
  }
}

bool HttpProxy::use_remote_proxy() {
  madoka::concurrent::ReadLock read_lock(&lock_);
  madoka::concurrent::LockGuard guard(&read_lock);

  return config_->use_remote_proxy() != 0;
}

const char* HttpProxy::remote_proxy_host() {
  madoka::concurrent::ReadLock read_lock(&lock_);
  madoka::concurrent::LockGuard guard(&read_lock);

  return config_->remote_proxy_host().c_str();
}

int HttpProxy::remote_proxy_port() {
  madoka::concurrent::ReadLock read_lock(&lock_);
  madoka::concurrent::LockGuard guard(&read_lock);

  return config_->remote_proxy_port();
}

void HttpProxy::EndSession(HttpProxySession* session) {
  madoka::concurrent::WriteLock write_lock(&lock_);
  madoka::concurrent::LockGuard guard(&write_lock);

  sessions_.remove(session);
  if (sessions_.empty())
    empty_.WakeAll();
}

bool HttpProxy::OnAccepted(madoka::net::AsyncSocket* client) {
  madoka::concurrent::WriteLock write_lock(&lock_);
  madoka::concurrent::LockGuard guard(&write_lock);

  if (stopped_)
    return false;

  HttpProxySession* session = new HttpProxySession(this);
  if (session == nullptr)
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

void HttpProxy::SetBasicCredential() {
  std::string auth = config_->remote_proxy_user() + ':' +
                     config_->remote_proxy_password();

  DWORD buffer_size = (((auth.size() - 1) / 3) + 1) * 4;
  basic_credential_.resize(buffer_size);
  buffer_size = basic_credential_.capacity();

  ::CryptBinaryToStringA(reinterpret_cast<const BYTE*>(auth.c_str()),
                         auth.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                         &basic_credential_[0], &buffer_size);

  basic_credential_.insert(0, "Basic ");
}
