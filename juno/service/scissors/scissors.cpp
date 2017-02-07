// Copyright (c) 2013 dacci.org

#include "service/scissors/scissors.h"

#include <base/logging.h>

#include <memory>

#include "io/net/datagram.h"
#include "io/net/socket_channel.h"
#include "io/secure_channel.h"
#include "misc/schannel/schannel_credential.h"
#include "service/scissors/scissors_tcp_session.h"
#include "service/scissors/scissors_udp_session.h"
#include "service/scissors/scissors_unwrapping_session.h"
#include "service/scissors/scissors_wrapping_session.h"

namespace juno {
namespace service {
namespace scissors {

Scissors::Scissors()
    : stopped_(), config_(nullptr), not_connecting_(&lock_), empty_(&lock_) {}

Scissors::~Scissors() {
  Scissors::Stop();
}

bool Scissors::UpdateConfig(const ServiceConfig* config) {
  base::AutoLock guard(lock_);

  while (!connecting_.empty())
    not_connecting_.Wait();

  config_ = static_cast<const ScissorsConfig*>(config);

  if (config_->remote_udp_)
    resolver_.SetType(SOCK_DGRAM);
  else
    resolver_.SetType(SOCK_STREAM);

  auto result =
      resolver_.Resolve(config_->remote_address_, config_->remote_port_);
  if (FAILED(result)) {
    LOG(ERROR) << "failed to resolve " << config_->remote_address_ << ":"
               << config_->remote_port_;
    return false;
  }

  if (config_->remote_ssl_ && credential_ == nullptr) {
    credential_ = std::make_unique<misc::schannel::SchannelCredential>();
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

std::unique_ptr<io::net::DatagramChannel> Scissors::CreateSocket() {
  auto socket = std::make_unique<io::net::DatagramChannel>();
  if (socket == nullptr)
    return nullptr;

  for (auto& end_point : resolver_) {
    if (socket->Connect(end_point.get()))
      break;
  }
  if (!socket->connected()) {
    LOG(ERROR) << "failed to connect to " << config_->remote_address_ << ":"
               << config_->remote_port_;
    return nullptr;
  }

  return socket;
}

std::unique_ptr<io::Channel> Scissors::CreateChannel(
    std::unique_ptr<io::Channel>&& channel) {
  if (channel == nullptr)
    return nullptr;

  base::AutoLock guard(lock_);

  if (!config_->remote_ssl_)
    return std::move(channel);

  auto secure_channel = std::make_unique<io::SecureChannel>(
      credential_.get(), std::move(channel), false);
  if (secure_channel == nullptr)
    return nullptr;

  secure_channel->context()->set_target_name(config_->remote_address_);

  return std::move(secure_channel);
}

HRESULT Scissors::ConnectSocket(io::net::SocketChannel* channel,
                                io::net::SocketChannel::Listener* listener) {
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

  lock_.Acquire();

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

  lock_.Release();

  removed.reset();
}

void Scissors::OnAccepted(std::unique_ptr<io::Channel>&& client) {
  if (stopped_)
    return;

  if (config_->remote_udp_)
    StartSession(
        std::make_unique<ScissorsUnwrappingSession>(this, std::move(client)));
  else
    StartSession(std::make_unique<ScissorsTcpSession>(this, std::move(client)));
}

void Scissors::OnReceivedFrom(std::unique_ptr<io::net::Datagram>&& datagram) {
  if (stopped_)
    return;

  UdpSession* udp_session;

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

    if (config_->remote_udp_)
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
    udp_session->OnReceived(std::move(datagram));
}

void Scissors::OnConnected(io::net::SocketChannel* socket, HRESULT result) {
  LOG_IF(ERROR, FAILED(result))
      << "failed to connect to " << config_->remote_address_ << ":"
      << config_->remote_port_ << " (error: 0x" << std::hex << result << ")";

  lock_.Acquire();

  auto listener = connecting_[socket];
  connecting_.erase(socket);
  if (connecting_.empty())
    not_connecting_.Broadcast();

  lock_.Release();

  listener->OnConnected(socket, result);
}

}  // namespace scissors
}  // namespace service
}  // namespace juno
