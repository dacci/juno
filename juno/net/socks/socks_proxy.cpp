// Copyright (c) 2013 dacci.org

#include "net/socks/socks_proxy.h"

#include <madoka/concurrent/lock_guard.h>

#include "net/async_socket.h"
#include "net/socks/socks_proxy_session.h"

SocksProxy::SocksProxy() : stopped_() {
  empty_event_ = ::CreateEvent(NULL, TRUE, TRUE, NULL);
}

SocksProxy::~SocksProxy() {
  ::CloseHandle(empty_event_);
}

bool SocksProxy::Setup(HKEY key) {
  return true;
}

void SocksProxy::Stop() {
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

void SocksProxy::EndSession(SocksProxySession* session) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  sessions_.remove(session);

  if (sessions_.empty())
    ::SetEvent(empty_event_);
}

bool SocksProxy::OnAccepted(AsyncSocket* client) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  if (stopped_)
    return false;

  SocksProxySession* session = new SocksProxySession(this);
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

void SocksProxy::OnReceivedFrom(AsyncDatagramSocket* socket, void* data,
                                int length, sockaddr* from, int from_length) {
}

void SocksProxy::OnError(DWORD error) {
}
