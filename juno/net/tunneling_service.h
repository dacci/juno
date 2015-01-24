// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_TUNNELING_SERVICE_H_
#define JUNO_NET_TUNNELING_SERVICE_H_

#include <windows.h>

#include <madoka/concurrent/condition_variable.h>
#include <madoka/concurrent/critical_section.h>

#include <memory>
#include <utility>
#include <vector>

class Channel;

class TunnelingService {
 public:
  typedef std::shared_ptr<Channel> ChannelPtr;

  static bool Init();
  static void Term();
  static bool Bind(const ChannelPtr& a, const ChannelPtr& b);

 private:
  class Session;

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
