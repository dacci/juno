// Copyright (c) 2013 dacci.org

#include "net/http/http_proxy.h"

#include <madoka/concurrent/lock_guard.h>

#include <utility>

#include "misc/registry_key-inl.h"
#include "net/async_server_socket.h"
#include "net/http/http_proxy_config.h"
#include "net/http/http_proxy_session.h"

HttpProxy::HttpProxy(HttpProxyConfig* config) : config_(config), stopped_() {
  empty_event_ = ::CreateEvent(NULL, TRUE, TRUE, NULL);
}

HttpProxy::~HttpProxy() {
  Stop();
  ::CloseHandle(empty_event_);
}

bool HttpProxy::Init() {
  return true;
}

void HttpProxy::Stop() {
  madoka::concurrent::LockGuard lock(&critical_section_);

  stopped_ = true;

  for (auto i = sessions_.begin(), l = sessions_.end(); i != l; ++i)
    (*i)->Stop();

  while (true) {
    critical_section_.Unlock();
    ::WaitForSingleObject(empty_event_, INFINITE);
    critical_section_.Lock();

    if (sessions_.empty())
      break;
  }
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

void HttpProxy::EndSession(HttpProxySession* session) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  sessions_.remove(session);
  if (sessions_.empty())
    ::SetEvent(empty_event_);
}

bool HttpProxy::OnAccepted(AsyncSocket* client) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  if (stopped_)
    return false;

  HttpProxySession* session = new HttpProxySession(this, config_);
  if (session == NULL)
    return false;

  sessions_.push_back(session);
  ::ResetEvent(empty_event_);

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
