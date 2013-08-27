// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_TUNNELING_SERVICE_H_
#define JUNO_NET_TUNNELING_SERVICE_H_

#include <madoka/concurrent/critical_section.h>

#include <map>

#include "net/async_socket.h"

class TunnelingService {
 public:
  static bool Init();
  static void Term();
  static bool Bind(AsyncSocket* a, AsyncSocket* b);

 private:
  class Session : AsyncSocket::Listener {
   public:
    static const size_t kBufferSize = 8192;

    Session(TunnelingService* service, AsyncSocket* from, AsyncSocket* to);
    ~Session();

    bool Start();

    void OnConnected(AsyncSocket* socket, DWORD error);
    void OnReceived(AsyncSocket* socket, DWORD error, int length);
    void OnSent(AsyncSocket* socket, DWORD error, int length);

    TunnelingService* service_;
    AsyncSocket* from_;
    AsyncSocket* to_;
    char* buffer_;
  };

  TunnelingService();
  ~TunnelingService();

  bool BindSocket(AsyncSocket* from, AsyncSocket* to);
  void EndSession(AsyncSocket* from, AsyncSocket* to);

  static TunnelingService* instance_;

  HANDLE empty_event_;
  madoka::concurrent::CriticalSection critical_section_;
  bool stopped_;

  std::map<AsyncSocket*, Session*> session_map_;
  std::map<AsyncSocket*, int> count_map_;
};

#endif  // JUNO_NET_TUNNELING_SERVICE_H_
