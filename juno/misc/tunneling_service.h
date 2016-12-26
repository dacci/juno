// Copyright (c) 2013 dacci.org

#ifndef JUNO_MISC_TUNNELING_SERVICE_H_
#define JUNO_MISC_TUNNELING_SERVICE_H_

#include <windows.h>

#include <base/synchronization/condition_variable.h>
#include <base/synchronization/lock.h>

#include <memory>
#include <utility>
#include <vector>

#include "io/channel.h"

namespace juno {
namespace misc {

class TunnelingService {
 public:
  static bool Init();
  static void Term();
  static bool Bind(const io::ChannelPtr& a, const io::ChannelPtr& b);

 private:
  class Session;

  typedef std::pair<TunnelingService*, Session*> ServiceSessionPair;

  TunnelingService();
  ~TunnelingService();

  bool BindSocket(const io::ChannelPtr& from, const io::ChannelPtr& to);

  void EndSession(Session* session);
  static void CALLBACK EndSessionImpl(PTP_CALLBACK_INSTANCE instance,
                                      void* param);
  void EndSessionImpl(Session* session);

  static TunnelingService* instance_;

  base::Lock lock_;
  base::ConditionVariable empty_;
  bool stopped_;

  std::vector<std::unique_ptr<Session>> sessions_;

  TunnelingService(const TunnelingService&) = delete;
  TunnelingService& operator=(const TunnelingService&) = delete;
};

}  // namespace misc
}  // namespace juno

#endif  // JUNO_MISC_TUNNELING_SERVICE_H_
