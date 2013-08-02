// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_TUNNELING_SERVICE_H_
#define JUNO_NET_TUNNELING_SERVICE_H_

#include <map>

#include "net/async_socket.h"

class TunnelingService : AsyncSocket::Listener {
 public:
  static bool Bind(AsyncSocket* a, AsyncSocket* b);

  void OnConnected(AsyncSocket* socket, DWORD error);
  void OnReceived(AsyncSocket* socket, DWORD error, int length);
  void OnSent(AsyncSocket* socket, DWORD error, int length);

 private:
  static const size_t kBufferSize = 8192;

  struct BindInfo {
    AsyncSocket* a;
    AsyncSocket* b;
    LONG ref_count;
  };

  struct SocketInfo {
    AsyncSocket* self;
    AsyncSocket* other;
    char* buffer;
  };

  typedef std::map<AsyncSocket*, BindInfo*> BindMap;
  typedef BindMap::value_type BindEntry;

  typedef std::map<AsyncSocket*, SocketInfo*> SocketMap;
  typedef SocketMap::value_type SocketEntry;

  TunnelingService();
  ~TunnelingService();

  bool BindImpl(AsyncSocket* a, AsyncSocket* b);

  static TunnelingService instance_;

  CRITICAL_SECTION critical_section_;
  BindMap bind_map_;
  SocketMap socket_map_;
};

#endif  // JUNO_NET_TUNNELING_SERVICE_H_
