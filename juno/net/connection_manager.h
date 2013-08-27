// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_CONNECTION_MANAGER_H_
#define JUNO_NET_CONNECTION_MANAGER_H_

#include <madoka/concurrent/critical_section.h>
#include <madoka/net/address_info.h>

#include <list>
#include <map>
#include <string>

#include "net/async_socket.h"

class ConnectionManager : public AsyncSocket::Listener {
 public:
  ConnectionManager();
  ~ConnectionManager();

  bool ConnectAsync(const char* host, int port,
                    AsyncSocket::Listener* listener);
  void Release(AsyncSocket* socket);

  void OnConnected(AsyncSocket* socket, DWORD error);
  void OnReceived(AsyncSocket* socket, DWORD error, int length);
  void OnSent(AsyncSocket* socket, DWORD error, int length);

 private:
  static const int kMaxCache = 8;

  struct CacheEntry {
    CacheEntry() : allocated() {
    }

    int allocated;
    std::list<AsyncSocket*> sockets;
    std::list<AsyncSocket::Listener*> waiting;
  };

  struct EndPoint {
    std::string host;
    int port;
  };

  struct PendingRequest {
    EndPoint end_point;
    AsyncSocket::Listener* listener;
    madoka::net::AddressInfo resolver;
  };

  struct EventArgs {
    AsyncSocket::Listener* listener;
    AsyncSocket* socket;
    DWORD error;
  };

  typedef std::map<int, CacheEntry> PortMap;
  typedef PortMap::value_type PortEntry;
  typedef std::map<std::string, PortMap> HostMap;
  typedef HostMap::value_type HostEntry;
  typedef std::map<AsyncSocket*, EndPoint> SocketMap;
  typedef SocketMap::value_type SocketEntry;
  typedef std::map<AsyncSocket*, PendingRequest*> PendingMap;
  typedef PendingMap::value_type PendingEntry;

  bool FireConnected(AsyncSocket::Listener* listener,
                     AsyncSocket* socket, DWORD error);
  static DWORD CALLBACK FireConnected(void* param);

  madoka::concurrent::CriticalSection critical_section_;
  HostMap cache_;
  SocketMap sockets_;
  std::map<AsyncSocket*, PendingRequest*> pending_;
};

#endif  // JUNO_NET_CONNECTION_MANAGER_H_
