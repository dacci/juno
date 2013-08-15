// Copyright (c) 2013 dacci.org

#include "net/connection_manager.h"

#include <assert.h>

ConnectionManager::ConnectionManager() {
  ::InitializeCriticalSection(&critical_section_);
}

ConnectionManager::~ConnectionManager() {
  ::DeleteCriticalSection(&critical_section_);
}

bool ConnectionManager::ConnectAsync(const char* host, int port,
                                     AsyncSocket::Listener* listener) {
  ::EnterCriticalSection(&critical_section_);

  do {
    auto i = cache_.find(host);
    if (i == cache_.end())
      break;

    auto j = i->second.find(port);
    if (j == i->second.end())
      break;

    auto cache = j->second;
    if (!cache.sockets.empty()) {
      AsyncSocket* socket = cache.sockets.front();
      bool succeeded = FireConnected(listener, socket, 0);
      if (succeeded)
        cache.sockets.pop_front();

      ::LeaveCriticalSection(&critical_section_);
      return succeeded;
    }

    if (cache.allocated >= kMaxCache) {
      cache.waiting.push_back(listener);
      ::LeaveCriticalSection(&critical_section_);
      return true;
    }
  } while (false);

  ::LeaveCriticalSection(&critical_section_);

  PendingRequest* pending = NULL;
  AsyncSocket* socket = NULL;

  do {
    pending = new PendingRequest();
    if (pending == NULL)
      break;

    pending->end_point.host = host;
    pending->end_point.port = port;
    pending->listener = listener;
    pending->resolver.ai_socktype = SOCK_STREAM;
    if (!pending->resolver.Resolve(host, port))
      break;

    socket = new AsyncSocket();
    if (socket == NULL)
      break;

    ::EnterCriticalSection(&critical_section_);
    pending_.insert(PendingEntry(socket, pending));
    ::LeaveCriticalSection(&critical_section_);

    if (!socket->ConnectAsync(*pending->resolver, this))
      break;

    return true;
  } while (false);

  if (socket != NULL) {
    ::EnterCriticalSection(&critical_section_);
    pending_.erase(socket);
    ::LeaveCriticalSection(&critical_section_);

    delete socket;
  }

  if (pending != NULL)
    delete pending;

  return false;
}

void ConnectionManager::Release(AsyncSocket* socket) {
  ::EnterCriticalSection(&critical_section_);

  EndPoint& end_point = sockets_[socket];
  CacheEntry& cache = cache_[end_point.host][end_point.port];

  AsyncSocket::Listener* waiting = NULL;

  if (cache.waiting.empty()) {
    // TODO(dacci): set timeout
    cache.sockets.push_back(socket);
  } else {
    waiting = cache.waiting.front();
    cache.waiting.pop_front();
  }

  ::LeaveCriticalSection(&critical_section_);

  if (waiting != NULL)
    if (!FireConnected(waiting, socket, 0))
      Release(socket);
}

void ConnectionManager::OnConnected(AsyncSocket* socket, DWORD error) {
  ::EnterCriticalSection(&critical_section_);

  PendingRequest* pending = pending_[socket];
  pending_.erase(socket);

  if (error == 0) {
    CacheEntry& cache_entry =
        cache_[pending->end_point.host][pending->end_point.port];
    ++cache_entry.allocated;
    cache_entry.sockets.push_back(socket);

    sockets_.insert(SocketEntry(socket, pending->end_point));
  }

  ::LeaveCriticalSection(&critical_section_);

  FireConnected(pending->listener, socket, error);
  delete pending;
}

void ConnectionManager::OnReceived(AsyncSocket* socket, DWORD error,
                                   int length) {
  assert(false);
}

void ConnectionManager::OnSent(AsyncSocket* socket, DWORD error, int length) {
  assert(false);
}

bool ConnectionManager::FireConnected(AsyncSocket::Listener* listener,
                                      AsyncSocket* socket, DWORD error) {
  EventArgs* args = new EventArgs();
  if (args == NULL)
    return false;

  args->listener = listener;
  args->socket = socket;
  args->error = error;

  if (::QueueUserWorkItem(FireConnected, args, 0)) {
    return true;
  } else {
    delete args;
    return false;
  }
}

DWORD CALLBACK ConnectionManager::FireConnected(void* param) {
  EventArgs* args = static_cast<EventArgs*>(param);

  args->listener->OnConnected(args->socket, args->error);

  delete args;

  return 0;
}