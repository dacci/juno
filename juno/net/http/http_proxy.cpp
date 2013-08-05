// Copyright (c) 2013 dacci.org

#include "net/http/http_proxy.h"

#include "net/async_server_socket.h"
#include "net/http/http_proxy_session.h"

HttpProxy::HttpProxy() {
  empty_event_ = ::CreateEvent(NULL, TRUE, TRUE, NULL);
  ::InitializeCriticalSection(&critical_section_);
}

HttpProxy::~HttpProxy() {
  Stop();
  ::DeleteCriticalSection(&critical_section_);
  ::CloseHandle(empty_event_);
}

bool HttpProxy::Setup(HKEY key) {
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
