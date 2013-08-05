// Copyright (c) 2013 dacci.org

#include "net/socks/socks_proxy.h"

#include "net/async_socket.h"
#include "net/socks/socks_proxy_session.h"

SocksProxy::SocksProxy() {
  empty_event_ = ::CreateEvent(NULL, TRUE, TRUE, NULL);
  ::InitializeCriticalSection(&critical_section_);
}

SocksProxy::~SocksProxy() {
  ::CloseHandle(empty_event_);
  ::DeleteCriticalSection(&critical_section_);
}

bool SocksProxy::Setup(HKEY key) {
  return true;
}

void SocksProxy::Stop() {
}

bool SocksProxy::OnAccepted(AsyncSocket* client) {
  SocksProxySession* session = new SocksProxySession(this, client);
  if (session == NULL)
    return false;

  if (!session->Start()) {
    delete session;
    return false;
  }

  return true;
}

void SocksProxy::OnError(DWORD error) {
}