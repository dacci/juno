// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_H_

#include <madoka/concurrent/condition_variable.h>
#include <madoka/concurrent/critical_section.h>
#include <madoka/net/async_socket.h>
#include <madoka/net/resolver.h>
#include <madoka/net/socket_event_listener.h>

#include <base/hash.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "service/service.h"

class SchannelCredential;
class ScissorsConfig;
class ServiceConfig;

class Scissors : public Service, private madoka::net::SocketEventAdapter {
 public:
  typedef std::shared_ptr<madoka::net::AsyncSocket> AsyncSocketPtr;

  class Session {
   public:
    explicit Session(Scissors* service) : service_(service) {}
    virtual ~Session() {}

    virtual bool Start() = 0;
    virtual void Stop() = 0;

    Scissors* const service_;
  };

  class UdpSession : public Session {
   public:
    explicit UdpSession(Scissors* service) : Session(service) {}
    virtual ~UdpSession() {}

    virtual void OnReceived(const DatagramPtr& datagram) = 0;
  };

  Scissors();
  virtual ~Scissors();

  bool UpdateConfig(const ServiceConfigPtr& config) override;
  void Stop() override;

  bool StartSession(std::unique_ptr<Session>&& session);
  void EndSession(Session* session);

  AsyncSocketPtr CreateSocket();

  ChannelPtr CreateChannel(
      const std::shared_ptr<madoka::net::AsyncSocket>& socket);
  void ConnectSocket(madoka::net::AsyncSocket* socket,
                     madoka::net::SocketEventListener* listener);

  ScissorsConfig* config() const {
    return config_.get();
  }

 private:
  static const int kBufferSize = 8192;

  struct Hash {
    size_t operator()(const sockaddr_storage& address) const {
      return base::Hash(reinterpret_cast<const char*>(&address),
                        sizeof(address));
    }
  };

  struct EqualTo {
    bool operator()(const sockaddr_storage& a,
                    const sockaddr_storage& b) const {
      return memcmp(&a, &b, sizeof(a)) == 0;
    }
  };

  typedef std::unordered_map<sockaddr_storage, UdpSession*, Hash, EqualTo>
      UdpSessionMap;

  static void CALLBACK EndSessionImpl(PTP_CALLBACK_INSTANCE instance,
                                      void* param);
  void EndSessionImpl(Session* session);

  void OnAccepted(const ChannelPtr& client) override;
  void OnReceivedFrom(const DatagramPtr& datagram) override;
  void OnError(DWORD error) override;
  void OnConnected(madoka::net::AsyncSocket* socket, DWORD error) override;

  bool stopped_;
  std::shared_ptr<ScissorsConfig> config_;
  std::unique_ptr<SchannelCredential> credential_;
  madoka::concurrent::CriticalSection lock_;

  madoka::net::Resolver resolver_;
  std::map<madoka::net::AsyncSocket*, madoka::net::SocketEventListener*>
      connecting_;
  madoka::concurrent::ConditionVariable not_connecting_;

  std::vector<std::unique_ptr<Session>> sessions_;
  madoka::concurrent::ConditionVariable empty_;

  UdpSessionMap udp_sessions_;
};

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_H_
