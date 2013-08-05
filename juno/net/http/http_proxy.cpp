// Copyright (c) 2013 dacci.org

#include "net/http/http_proxy.h"

#include <atlbase.h>
#include <atlstr.h>

#include "net/async_server_socket.h"
#include "net/http/http_proxy_session.h"

HttpProxy::HttpProxy() : use_remote_proxy_(), remote_proxy_port_() {
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

  reg_key.Detach();

  return true;
}

void HttpProxy::Stop() {
  ::EnterCriticalSection(&critical_section_);
  for (auto i = sessions_.begin(), l = sessions_.end(); i != l; ++i)
    (*i)->Stop();
  ::LeaveCriticalSection(&critical_section_);

  ::WaitForSingleObject(empty_event_, INFINITE);
}

void HttpProxy::EndSession(HttpProxySession* session) {
  ::EnterCriticalSection(&critical_section_);
  sessions_.remove(session);
  if (sessions_.empty())
    ::SetEvent(empty_event_);
  ::LeaveCriticalSection(&critical_section_);
}

bool HttpProxy::OnAccepted(AsyncSocket* client) {
  HttpProxySession* session = new HttpProxySession(this, client);
  if (session == NULL)
    return false;

  ::EnterCriticalSection(&critical_section_);
  sessions_.push_back(session);
  ::ResetEvent(empty_event_);
  ::LeaveCriticalSection(&critical_section_);

  if (!session->Start()) {
    delete session;
    return false;
  }

  return true;
}

void HttpProxy::OnError(DWORD error) {
}
