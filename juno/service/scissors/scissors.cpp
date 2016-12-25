// Copyright (c) 2013 dacci.org

#include "service/scissors/scissors.h"

#include <base/logging.h>

#include "io/net/datagram.h"
#include "io/net/socket_channel.h"
#include "io/secure_channel.h"
#include "misc/registry_key.h"
#include "misc/schannel/schannel_credential.h"
#include "service/scissors/scissors_config.h"
#include "service/scissors/scissors_tcp_session.h"
#include "service/scissors/scissors_udp_session.h"
#include "service/scissors/scissors_unwrapping_session.h"
#include "service/scissors/scissors_wrapping_session.h"

Scissors::Scissors() : stopped_(), not_connecting_(&lock_), empty_(&lock_) {}

Scissors::~Scissors() {
  Stop();
}

bool Scissors::UpdateConfig(const ServiceConfigPtr& config) {
  base::AutoLock guard(lock_);

  while (!connecting_.empty())
    not_connecting_.Wait();

  config_ = std::static_pointer_cast<ScissorsConfig>(config);

  if (config_->remote_udp())
    resolver_.SetType(SOCK_DGRAM);
  else
    resolver_.SetType(SOCK_STREAM);

  auto result =
      resolver_.Resolve(config_->remote_address(), config_->remote_port());
  if (FAILED(result)) {
    LOG(ERROR) << "failed to resolve " << config_->remote_address() << ":"
               << config_->remote_port();
    return false;
  }

  if (config_->remote_ssl() && credential_ == nullptr) {
    credential_.reset(new SchannelCredential());
    if (credential_ == nullptr)
      return false;

    credential_->SetEnabledProtocols(SP_PROT_SSL3TLS1_X_CLIENTS);
    credential_->SetFlags(SCH_CRED_MANUAL_CRED_VALIDATION);

    auto status = credential_->AcquireHandle(SECPKG_CRED_OUTBOUND);
    if (FAILED(status)) {
      LOG(ERROR) << "failed to initialize credential: " << status;
      return false;
    }
  }

  return true;
}

void Scissors::Stop() {
  base::AutoLock guard(lock_);

  stopped_ = true;

  for (auto& session : sessions_)
    session->Stop();

  while (!sessions_.empty())
    empty_.Wait();
}

bool Scissors::StartSession(std::unique_ptr<Session>&& session) {
  if (session == nullptr)
    return false;

  if (!session->Start())
    return false;

  lock_.Acquire();
  sessions_.push_back(std::move(session));
  lock_.Release();

  return true;
}

void Scissors::EndSession(Session* session) {
  DLOG(INFO) << session << " " __FUNCTION__;

  if (!TrySubmitThreadpoolCallback(EndSessionImpl, session, nullptr))
    EndSessionImpl(session);
}

std::shared_ptr<DatagramChannel> Scissors::CreateSocket() {
  auto socket = std::make_shared<DatagramChannel>();
  if (socket == nullptr)
    return nullptr;

  for (auto& end_point : resolver_) {
    if (socket->Connect(end_point.get()))
      break;
  }
  if (!socket->connected()) {
    LOG(ERROR) << "failed to connect to " << config_->remote_address() << ":"
               << config_->remote_port();
    return nullptr;
  }

  return socket;
}

ChannelPtr Scissors::CreateChannel(const ChannelPtr& channel) {
  if (channel == nullptr)
    return channel;

  base::AutoLock guard(lock_);

  if (!config_->remote_ssl())
    return channel;

  auto secure_channel =
      std::make_shared<SecureChannel>(credential_.get(), channel, false);
  if (secure_channel == nullptr)
    return channel;

  secure_channel->context()->set_target_name(config_->remote_address());

  return secure_channel;
}

HRESULT Scissors::ConnectSocket(SocketChannel* channel,
                                SocketChannel::Listener* listener) {
  base::AutoLock guard(lock_);

  connecting_.insert({channel, listener});
  return channel->ConnectAsync(resolver_.begin()->get(), this);
}

void CALLBACK Scissors::EndSessionImpl(PTP_CALLBACK_INSTANCE /*instance*/,
                                       void* param) {
  auto session = static_cast<Session*>(param);
  session->service_->EndSessionImpl(session);
}

void Scissors::EndSessionImpl(Session* session) {
  std::unique_ptr<Session> removed;
  base::AutoLock guard(lock_);

  for (auto i = sessions_.begin(), l = sessions_.end(); i != l; ++i) {
    if (i->get() == session) {
      removed = std::move(*i);
      sessions_.erase(i);

      if (sessions_.empty())
        empty_.Broadcast();

      break;
    }
  }

  for (auto i = udp_sessions_.begin(), l = udp_sessions_.end(); i != l; ++i) {
    if (i->second == session) {
      udp_sessions_.erase(i);
      break;
    }
  }
}

void Scissors::OnAccepted(const ChannelPtr& client) {
  if (stopped_)
    return;

  if (config_->remote_udp())
    StartSession(std::make_unique<ScissorsUnwrappingSession>(this, client));
  else
    StartSession(std::make_unique<ScissorsTcpSession>(this, client));
}

void Scissors::OnReceivedFrom(const DatagramPtr& datagram) {
  if (stopped_)
    return;

  UdpSession* udp_session = nullptr;

  sockaddr_storage key;
  memset(&key, 0, sizeof(key));
  memmove(&key, &datagram->from, datagram->from_length);

  lock_.Acquire();
  auto found = udp_sessions_.find(key);
  auto end = udp_sessions_.end();
  lock_.Release();

  if (found == end) {
    LOG(INFO) << this << " session not found, creating new one";

    std::unique_ptr<UdpSession> session;

    if (config_->remote_udp())
      session = std::make_unique<ScissorsUdpSession>(this, datagram->channel);
    else
      session = std::move(
          std::make_unique<ScissorsWrappingSession>(this, datagram.get()));

    udp_session = session.get();
    if (StartSession(std::move(session))) {
      base::AutoLock guard(lock_);
      udp_sessions_.insert({key, udp_session});
    } else {
      udp_session = nullptr;
    }
  } else {
    DLOG(INFO) << this << " session found";

    udp_session = found->second;
  }

  if (udp_session != nullptr)
    udp_session->OnReceived(datagram);
}

void Scissors::OnConnected(SocketChannel* socket, HRESULT result) {
  LOG_IF(ERROR, FAILED(result))
      << "failed to connect to " << config_->remote_address() << ":"
      << config_->remote_port() << " (error: 0x" << std::hex << result << ")";

  lock_.Acquire();

  auto listener = connecting_[socket];
  connecting_.erase(socket);
  if (connecting_.empty())
    not_connecting_.Broadcast();

  lock_.Release();

  listener->OnConnected(socket, result);
}
