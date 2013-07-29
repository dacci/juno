// Copyright (c) 2013 dacci.org

#include "net/tunneling_service.h"

#include <assert.h>

TunnelingService TunnelingService::instance_;

bool TunnelingService::Bind(AsyncSocket* a, AsyncSocket* b) {
  return instance_.BindImpl(a, b);
}

void TunnelingService::OnConnected(AsyncSocket* socket, DWORD error) {
  assert(false);
}

void TunnelingService::OnReceived(AsyncSocket* socket, DWORD error,
                                  int length) {
  ::EnterCriticalSection(&critical_section_);
  BindInfo* bind_info = bind_map_[socket];
  SocketInfo* socket_info = socket_map_[socket];
  ::LeaveCriticalSection(&critical_section_);

  if (error == 0 && length > 0)
    if (socket_info->other->SendAsync(socket_info->buffer, length, 0, this))
      return;

  socket->Shutdown(SD_RECEIVE);
  socket_info->other->Shutdown(SD_SEND);

  ::EnterCriticalSection(&critical_section_);
  bind_map_.erase(socket);
  socket_map_.erase(socket);
  ::LeaveCriticalSection(&critical_section_);

  delete[] socket_info->buffer;
  delete socket_info;

  if (::InterlockedDecrement(&bind_info->ref_count) == 0) {
    delete bind_info->a;
    delete bind_info->b;
    delete bind_info;
  }
}

void TunnelingService::OnSent(AsyncSocket* socket, DWORD error, int length) {
  ::EnterCriticalSection(&critical_section_);
  BindInfo* bind_info = bind_map_[socket];
  SocketInfo* socket_info = socket_map_[socket];
  SocketInfo* pair_info = socket_map_[socket_info->other];
  ::LeaveCriticalSection(&critical_section_);

  if (error == 0 && length > 0)
    if (pair_info->self->ReceiveAsync(pair_info->buffer, kBufferSize, 0, this))
      return;

  socket->Shutdown(SD_RECEIVE);
  socket_info->other->Shutdown(SD_SEND);

  ::EnterCriticalSection(&critical_section_);
  bind_map_.erase(socket);
  socket_map_.erase(socket);
  ::LeaveCriticalSection(&critical_section_);

  delete[] socket_info->buffer;
  delete socket_info;

  if (::InterlockedDecrement(&bind_info->ref_count) == 0) {
    delete bind_info->a;
    delete bind_info->b;
    delete bind_info;
  }
}

TunnelingService::TunnelingService() {
  ::InitializeCriticalSection(&critical_section_);
}

TunnelingService::~TunnelingService() {
  ::DeleteCriticalSection(&critical_section_);
}

bool TunnelingService::BindImpl(AsyncSocket* a, AsyncSocket* b) {
  if (a == NULL || b == NULL)
    return false;
  if (!a->connected() || !b->connected())
    return false;

  BindInfo* bind_info = NULL;
  SocketInfo* socket_info_a = NULL;
  SocketInfo* socket_info_b = NULL;

  do {
    bind_info = new BindInfo();
    if (bind_info == NULL)
      break;

    bind_info->a = a;
    bind_info->b = b;
    bind_info->ref_count = 2;

    socket_info_a = new SocketInfo();
    if (socket_info_a == NULL)
      break;

    socket_info_a->self = a;
    socket_info_a->other = b;
    socket_info_a->buffer = new char[kBufferSize];
    if (socket_info_a->buffer == NULL)
      break;

    socket_info_b = new SocketInfo();
    if (socket_info_b == NULL)
      break;

    socket_info_b->self = b;
    socket_info_b->other = a;
    socket_info_b->buffer = new char[kBufferSize];
    if (socket_info_b->buffer == NULL)
      break;

    ::EnterCriticalSection(&critical_section_);
    bind_map_.insert(BindEntry(a, bind_info));
    bind_map_.insert(BindEntry(b, bind_info));

    socket_map_.insert(SocketEntry(a, socket_info_a));
    socket_map_.insert(SocketEntry(b, socket_info_b));
    ::LeaveCriticalSection(&critical_section_);

    if (!a->ReceiveAsync(socket_info_a->buffer, kBufferSize, 0, this))
      break;

    if (!b->ReceiveAsync(socket_info_b->buffer, kBufferSize, 0, this))
      break;

    return true;
  } while (false);

  a->Close();
  b->Close();

  bind_map_.erase(a);
  bind_map_.erase(b);

  socket_map_.erase(a);
  socket_map_.erase(b);

  if (socket_info_a != NULL) {
    if (socket_info_a->buffer != NULL)
      delete[] socket_info_a->buffer;
    delete socket_info_a;
  }

  if (socket_info_b != NULL) {
    if (socket_info_b->buffer != NULL)
      delete[] socket_info_b->buffer;
    delete socket_info_b;
  }

  if (bind_info != NULL)
    delete bind_info;

  return false;
}
