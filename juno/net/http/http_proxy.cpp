// Copyright (c) 2013 dacci.org

#include "net/http/http_proxy.h"

#include <atlbase.h>
#include <atlstr.h>

#include <utility>

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
  CRegKey reg_key(key);

  LONG result = reg_key.QueryDWORDValue("UseRemoteProxy", use_remote_proxy_);
  if (result != ERROR_SUCCESS)
    use_remote_proxy_ = FALSE;

  CString string;
  ULONG length = 256;
  result = reg_key.QueryStringValue("RemoteProxyHost", string.GetBuffer(length),
                                    &length);
  if (result == ERROR_SUCCESS) {
    string.ReleaseBuffer(length);
    remote_proxy_host_ = string.GetString();
  } else {
    use_remote_proxy_ = FALSE;
  }

  result = reg_key.QueryDWORDValue("RemoteProxyPort", remote_proxy_port_);
  if (result != ERROR_SUCCESS ||
      remote_proxy_port_ <= 0 || 65536 <= remote_proxy_port_)
    use_remote_proxy_ = FALSE;

  result = reg_key.QueryDWORDValue("AuthRemoteProxy", auth_remote_proxy_);
  if (result != ERROR_SUCCESS)
    auth_remote_proxy_ = 0;

  length = 256;
  result = reg_key.QueryStringValue("RemoteProxyAuth", string.GetBuffer(length),
                                    &length);
  if (result == ERROR_SUCCESS) {
    string.ReleaseBuffer(length);
    remote_proxy_auth_ = string.GetString();
  } else {
    auth_remote_proxy_ = 0;
  }

  CRegKey filters_key;
  if (filters_key.Open(reg_key, "HeaderFilters") == ERROR_SUCCESS) {
    for (DWORD i = 0; ; ++i) {
      CString name;
      DWORD length = 16;
      int result = filters_key.EnumKey(i, name.GetBuffer(length), &length);
      if (result != ERROR_SUCCESS)
        break;

      name.ReleaseBuffer(length);

      CRegKey filter_key;
      filter_key.Open(filters_key, name);

      HeaderFilter filter;
      DWORD dword_value;
      CString string_value;

      filter_key.QueryDWORDValue("Request", dword_value);
      filter.request = dword_value != 0;

      filter_key.QueryDWORDValue("Response", dword_value);
      filter.response = dword_value != 0;

      filter_key.QueryDWORDValue("Action", dword_value);
      filter.action = static_cast<FilterAction>(dword_value);

      length = 64;
      filter_key.QueryStringValue("Name", string_value.GetBuffer(length),
                                  &length);
      string_value.ReleaseBuffer(length);
      filter.name = string_value;

      length = 64;
      filter_key.QueryStringValue("Value", string_value.GetBuffer(length),
                                  &length);
      string_value.ReleaseBuffer(length);
      filter.value = string_value;

      length = 64;
      filter_key.QueryStringValue("Replace", string_value.GetBuffer(length),
                                  &length);
      string_value.ReleaseBuffer(length);
      filter.replace = string_value;

      header_filters_.push_back(std::move(filter));
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
      case Edit:
      case EditR:
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
