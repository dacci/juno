// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_TUNNELING_SERVICE_H_
#define JUNO_NET_TUNNELING_SERVICE_H_

#include <madoka/concurrent/condition_variable.h>
#include <madoka/concurrent/critical_section.h>
#include <madoka/net/async_socket.h>

#include <memory>
#include <utility>
#include <vector>

#include "net/channel.h"

typedef std::shared_ptr<madoka::net::AsyncSocket> AsyncSocketPtr;

class TunnelingService {
 public:
  typedef std::shared_ptr<Channel> ChannelPtr;

  static bool Init();
  static void Term();
  static bool Bind(const ChannelPtr& a, const ChannelPtr& b);
  // __declspec(deprecated)
  static bool Bind(const AsyncSocketPtr& a, const AsyncSocketPtr& b);

 private:
  class Session : public Channel::Listener {
   public:
    Session(TunnelingService* service, const ChannelPtr& from,
            const ChannelPtr& to);
    ~Session();

    void Start();

    void OnRead(Channel* channel, DWORD error, void* buffer,
                int length) override;
    void OnWritten(Channel* channel, DWORD error, void* buffer,
                   int length) override;

    static void CALLBACK EndSession(PTP_CALLBACK_INSTANCE instance,
                                    void* param);

    TunnelingService* service_;
    ChannelPtr from_;
    ChannelPtr to_;
    char buffer_[8192];
  };

  typedef std::pair<TunnelingService*, Session*> ServiceSessionPair;

  TunnelingService();
  ~TunnelingService();

  bool BindSocket(const ChannelPtr& from, const ChannelPtr& to);

  void EndSession(Session* session);
  static void CALLBACK EndSessionImpl(PTP_CALLBACK_INSTANCE instance,
                                      void* param);
  void EndSessionImpl(Session* session);

  static TunnelingService* instance_;

  madoka::concurrent::CriticalSection lock_;
  madoka::concurrent::ConditionVariable empty_;
  bool stopped_;

  std::vector<std::unique_ptr<Session>> sessions_;
};

#endif  // JUNO_NET_TUNNELING_SERVICE_H_
