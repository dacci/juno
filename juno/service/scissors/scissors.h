// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_H_

#pragma warning(push, 3)
#include <base/hash.h>
#include <base/synchronization/condition_variable.h>
#include <base/synchronization/lock.h>
#pragma warning(pop)

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "net/socket_channel.h"
#include "net/socket_resolver.h"
#include "service/service.h"

class DatagramChannel;
class SchannelCredential;
class ScissorsConfig;
class ServiceConfig;

class Scissors : public Service, private SocketChannel::Listener {
 public:
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
  ~Scissors();

  bool UpdateConfig(const ServiceConfigPtr& config) override;
  void Stop() override;

  bool StartSession(std::unique_ptr<Session>&& session);
  void EndSession(Session* session);

  std::shared_ptr<DatagramChannel> CreateSocket();

  ChannelPtr CreateChannel(const ChannelPtr& channel);
  HRESULT ConnectSocket(SocketChannel* channel,
                        SocketChannel::Listener* listener);

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
  void OnError(HRESULT /*result*/) override {}

  void OnConnected(SocketChannel* socket, HRESULT result) override;
  void OnClosed(SocketChannel* /*channel*/, HRESULT /*result*/) override {}

  bool stopped_;
  std::shared_ptr<ScissorsConfig> config_;
  std::unique_ptr<SchannelCredential> credential_;
  base::Lock lock_;

  SocketResolver resolver_;
  std::map<SocketChannel*, SocketChannel::Listener*> connecting_;
  base::ConditionVariable not_connecting_;

  std::vector<std::unique_ptr<Session>> sessions_;
  base::ConditionVariable empty_;

  UdpSessionMap udp_sessions_;

  Scissors(const Scissors&) = delete;
  Scissors& operator=(const Scissors&) = delete;
};

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_H_
