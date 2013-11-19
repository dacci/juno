// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_TUNNELING_SERVICE_H_
#define JUNO_NET_TUNNELING_SERVICE_H_

#include <madoka/concurrent/critical_section.h>

#include <list>

#include "net/async_socket.h"

class TunnelingService {
 public:
  static bool Init();
  static void Term();
  static bool Bind(const AsyncSocketPtr& a, const AsyncSocketPtr& b);

 private:
  class Session : AsyncSocket::Listener {
   public:
    static const size_t kBufferSize = 8192;

    Session(TunnelingService* service, const AsyncSocketPtr& from,
            const AsyncSocketPtr& to);
    ~Session();

    bool Start();

    void OnConnected(AsyncSocket* socket, DWORD error);
    void OnReceived(AsyncSocket* socket, DWORD error, int length);
    void OnSent(AsyncSocket* socket, DWORD error, int length);

    static void CALLBACK EndSession(PTP_CALLBACK_INSTANCE instance,
                                    void* param);

    TunnelingService* service_;
    AsyncSocketPtr from_;
    AsyncSocketPtr to_;
    char* buffer_;
  };

  TunnelingService();
  ~TunnelingService();

  bool BindSocket(const AsyncSocketPtr& from, const AsyncSocketPtr& to);
  void EndSession(Session* session);

  static TunnelingService* instance_;

  // TODO(dacci): replace with condition variable
  HANDLE empty_event_;
  madoka::concurrent::CriticalSection critical_section_;
  bool stopped_;

  std::list<Session*> sessions_;
};

#endif  // JUNO_NET_TUNNELING_SERVICE_H_
