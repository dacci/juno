// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_TUNNELING_SERVICE_H_
#define JUNO_NET_TUNNELING_SERVICE_H_

#include <madoka/concurrent/condition_variable.h>
#include <madoka/concurrent/critical_section.h>
#include <madoka/net/async_socket.h>
#include <madoka/net/socket_event_listener.h>

#include <list>
#include <memory>

typedef std::shared_ptr<madoka::net::AsyncSocket> AsyncSocketPtr;

class TunnelingService {
 public:
  static bool Init();
  static void Term();
  static bool Bind(const AsyncSocketPtr& a, const AsyncSocketPtr& b);

 private:
  class Session : public madoka::net::SocketEventAdapter {
   public:
    static const size_t kBufferSize = 8192;

    Session(TunnelingService* service, const AsyncSocketPtr& from,
            const AsyncSocketPtr& to);
    ~Session();

    bool Start();

    void OnReceived(madoka::net::AsyncSocket* socket, DWORD error, int length);
    void OnSent(madoka::net::AsyncSocket* socket, DWORD error, int length);

    static void CALLBACK EndSession(PTP_CALLBACK_INSTANCE instance,
                                    void* param);

    TunnelingService* service_;
    AsyncSocketPtr from_;
    AsyncSocketPtr to_;
    std::unique_ptr<char[]> buffer_;
  };

  TunnelingService();
  ~TunnelingService();

  bool BindSocket(const AsyncSocketPtr& from, const AsyncSocketPtr& to);
  void EndSession(Session* session);

  static TunnelingService* instance_;

  madoka::concurrent::ConditionVariable empty_;
  madoka::concurrent::CriticalSection critical_section_;
  bool stopped_;

  std::list<Session*> sessions_;
};

#endif  // JUNO_NET_TUNNELING_SERVICE_H_
