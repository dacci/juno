// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_H_

#include <madoka/concurrent/condition_variable.h>
#include <madoka/concurrent/critical_section.h>

#include <list>
#include <map>
#include <memory>
#include <string>

#include "service/service.h"

class SchannelCredential;
class ScissorsConfig;
class ServiceConfig;

class Scissors : public Service {
 public:
  class Session {
   public:
    virtual ~Session() {}

    virtual void Stop() = 0;
  };

  explicit Scissors(const std::shared_ptr<ServiceConfig>& config);
  virtual ~Scissors();

  bool Init();
  bool UpdateConfig(const ServiceConfigPtr& config) override;
  void Stop() override;

  bool OnAccepted(const ChannelPtr& client) override {
    return false;
  }

  bool OnAccepted(const AsyncSocketPtr& client);
  bool OnReceivedFrom(Datagram* datagram) override;
  void OnError(DWORD error) override;

 private:
  friend class ScissorsProvider;
  friend class ScissorsTcpSession;
  friend class ScissorsUdpSession;

  static const int kBufferSize = 8192;

  void EndSession(Session* session);

  std::shared_ptr<ScissorsConfig> config_;

  madoka::concurrent::ConditionVariable empty_;
  madoka::concurrent::CriticalSection critical_section_;
  std::list<Session*> sessions_;

  std::map<madoka::net::AsyncDatagramSocket*, Datagram*> datagrams_;
  std::map<madoka::net::AsyncDatagramSocket*, char*> buffers_;

  bool stopped_;

  std::unique_ptr<SchannelCredential> credential_;
};

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_H_
