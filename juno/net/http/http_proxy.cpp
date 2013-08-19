// Copyright (c) 2013 dacci.org

#include "net/http/http_proxy.h"

#include <utility>

#include "misc/registry_key-inl.h"
#include "net/async_server_socket.h"
#include "net/http/http_proxy_session.h"

HttpProxy::HttpProxy() : stopped_(), use_remote_proxy_(), remote_proxy_port_() {
  empty_event_ = ::CreateEvent(NULL, TRUE, TRUE, NULL);
  ::InitializeCriticalSection(&critical_section_);
}

HttpProxy::~HttpProxy() {
  Stop();
  ::DeleteCriticalSection(&critical_section_);
  ::CloseHandle(empty_event_);
}

bool HttpProxy::Setup(HKEY key) {
  RegistryKey reg_key(key);

  use_remote_proxy_ = reg_key.QueryInteger("UseRemoteProxy");

  if (!reg_key.QueryString("RemoteProxyHost", &remote_proxy_host_))
    use_remote_proxy_ = FALSE;

  remote_proxy_port_ = reg_key.QueryInteger("RemoteProxyPort");
  if (remote_proxy_port_ <= 0 || 65536 <= remote_proxy_port_)
    use_remote_proxy_ = FALSE;

  auth_remote_proxy_ = reg_key.QueryInteger("AuthRemoteProxy");

  if (!reg_key.QueryString("RemoteProxyAuth", &remote_proxy_auth_))
    auth_remote_proxy_ = 0;

  RegistryKey filters_key;
  if (filters_key.Open(reg_key, "HeaderFilters")) {
    for (DWORD i = 0; ; ++i) {
      std::string name;
      if (!filters_key.EnumerateKey(i, &name))
        break;

      RegistryKey filter_key;
      filter_key.Open(filters_key, name);

      header_filters_.push_back(HeaderFilter());
      HeaderFilter& filter = header_filters_.back();

      filter.request = filter_key.QueryInteger("Request") != 0;
      filter.response = filter_key.QueryInteger("Response") != 0;
      filter.action =
          static_cast<FilterAction>(filter_key.QueryInteger("Action"));
      filter_key.QueryString("Name", &filter.name);
      filter_key.QueryString("Value", &filter.value);
      filter_key.QueryString("Replace", &filter.replace);
    }

    filters_key.Close();
  }

  reg_key.Detach();

  return true;
}

void HttpProxy::Stop() {
  stopped_ = true;

  ::EnterCriticalSection(&critical_section_);
  for (auto i = sessions_.begin(), l = sessions_.end(); i != l; ++i)
    (*i)->Stop();
  ::LeaveCriticalSection(&critical_section_);

  ::WaitForSingleObject(empty_event_, INFINITE);
}

void HttpProxy::FilterHeaders(HttpHeaders* headers, bool request) {
  for (auto i = header_filters_.begin(), l = header_filters_.end();
       i != l; ++i) {
    HeaderFilter& filter = *i;

    if (request && filter.request || !request && filter.response) {
      switch (filter.action) {
      case Set:
        headers->SetHeader(filter.name, filter.value);
        break;

      case Append:
        headers->AppendHeader(filter.name, filter.value);
        break;

      case Add:
        headers->AddHeader(filter.name, filter.value);
        break;

      case Unset:
        headers->RemoveHeader(filter.name);
        break;

      case Merge:
        headers->MergeHeader(filter.name, filter.value);
        break;

      case Edit:
        headers->EditHeader(filter.name, filter.value, filter.replace, false);
        break;

      case EditR:
        headers->EditHeader(filter.name, filter.value, filter.replace, true);
        break;
      }
    }
  }
}

void HttpProxy::EndSession(HttpProxySession* session) {
  ::EnterCriticalSection(&critical_section_);
  sessions_.remove(session);
  if (sessions_.empty())
    ::SetEvent(empty_event_);
  ::LeaveCriticalSection(&critical_section_);
}

bool HttpProxy::OnAccepted(AsyncSocket* client) {
  if (stopped_)
    return false;

  HttpProxySession* session = new HttpProxySession(this, client);
  if (session == NULL)
    return false;

  ::EnterCriticalSection(&critical_section_);
  sessions_.push_back(session);
  ::ResetEvent(empty_event_);

  bool started = session->Start();
  if (!started) {
    EndSession(session);
    delete session;
  }

  ::LeaveCriticalSection(&critical_section_);

  return started;
}

void HttpProxy::OnError(DWORD error) {
}
