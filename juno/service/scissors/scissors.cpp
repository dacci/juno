// Copyright (c) 2013 dacci.org

#include "service/scissors/scissors.h"

#include <madoka/concurrent/lock_guard.h>

#include <base/logging.h>

#include "misc/registry_key.h"
#include "misc/schannel/schannel_credential.h"
#include "net/datagram.h"
#include "net/secure_socket_channel.h"
#include "net/socket_channel.h"
#include "net/tunneling_service.h"
#include "service/scissors/scissors_config.h"
#include "service/scissors/scissors_tcp_session.h"
#include "service/scissors/scissors_udp_session.h"
#include "service/scissors/scissors_unwrapping_session.h"
#include "service/scissors/scissors_wrapping_session.h"

using ::madoka::net::AsyncSocket;
using ::madoka::net::Resolver;

Scissors::Scissors() : stopped_() {
}

Scissors::~Scissors() {
  Stop();
}

bool Scissors::UpdateConfig(const ServiceConfigPtr& config) {
  madoka::concurrent::LockGuard lock(&lock_);

  while (!connecting_.empty())
    not_connecting_.Sleep(&lock_);

  config_ = std::static_pointer_cast<ScissorsConfig>(config);

  if (config_->remote_udp())
    resolver_.SetType(SOCK_DGRAM);
  else
    resolver_.SetType(SOCK_STREAM);

  if (!resolver_.Resolve(config_->remote_address(), config_->remote_port())) {
    LOG(ERROR) << "failed to resolve "
               << config_->remote_address() << ":" << config_->remote_port();
    return false;
  }

  if (config_->remote_ssl() && credential_ == nullptr) {
    credential_.reset(new SchannelCredential());
    if (credential_ == nullptr)
      return false;

    credential_->SetEnabledProtocols(SP_PROT_SSL3TLS1_X_CLIENTS);
    credential_->SetFlags(SCH_CRED_MANUAL_CRED_VALIDATION);

    SECURITY_STATUS status = credential_->AcquireHandle(SECPKG_CRED_OUTBOUND);
    if (FAILED(status)) {
      LOG(ERROR) << "failed to initialize credential: " << status;
      return false;
    }
  }

  return true;
}

void Scissors::Stop() {
  madoka::concurrent::LockGuard lock(&lock_);

  stopped_ = true;

  for (auto& session : sessions_)
    session->Stop();

  while (!sessions_.empty())
    empty_.Sleep(&lock_);
}

bool Scissors::StartSession(std::unique_ptr<Session>&& session) {
  if (session == nullptr)
    return false;

  madoka::concurrent::LockGuard lock(&lock_);

  if (!session->Start())
    return false;

  sessions_.push_back(std::move(session));

  return true;
}

void Scissors::EndSession(Session* session) {
  DLOG(INFO) << session << " " __FUNCTION__;

  if (!TrySubmitThreadpoolCallback(EndSessionImpl, session, nullptr))
    EndSessionImpl(session);
}

Scissors::AsyncSocketPtr Scissors::CreateSocket() {
  auto socket = std::make_shared<AsyncSocket>();
  if (socket == nullptr)
    return nullptr;

  for (auto end_point : resolver_) {
    if (socket->Connect(end_point))
      break;
  }
  if (!socket->connected()) {
    LOG(ERROR) << "failed to connect to "
               << config_->remote_address() << ":" << config_->remote_port();
    return nullptr;
  }

  return socket;
}

Service::ChannelPtr Scissors::CreateChannel(
    const std::shared_ptr<AsyncSocket>& socket) {
  madoka::concurrent::LockGuard lock(&lock_);

  if (config_->remote_ssl()) {
    auto channel =
        std::make_shared<SecureSocketChannel>(credential_.get(), socket, false);
    if (channel != nullptr)
      channel->context()->set_target_name(config_->remote_address());
    return channel;
  } else {
    return std::make_shared<SocketChannel>(socket);
  }
}

void Scissors::ConnectSocket(AsyncSocket* socket,
                             AsyncSocket::Listener* listener) {
  madoka::concurrent::LockGuard lock(&lock_);

  connecting_.insert({ socket, listener });
  socket->ConnectAsync(*resolver_.begin(), this);
}

void CALLBACK Scissors::EndSessionImpl(PTP_CALLBACK_INSTANCE instance,
                                       void* param) {
  Session* session = static_cast<Session*>(param);
  session->service_->EndSessionImpl(session);
}

void Scissors::EndSessionImpl(Session* session) {
  std::unique_ptr<Session> removed;
  madoka::concurrent::LockGuard lock(&lock_);

  for (auto i = sessions_.begin(), l = sessions_.end(); i != l; ++i) {
    if (i->get() == session) {
      removed = std::move(*i);
      sessions_.erase(i);

      if (sessions_.empty())
        empty_.WakeAll();

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
  madoka::concurrent::LockGuard lock(&lock_);

  if (stopped_)
    return;

  if (config_->remote_udp()) {
    auto session = std::make_unique<ScissorsUnwrappingSession>(this);
    if (session == nullptr)
      return;

    session->SetSource(client);

    StartSession(std::move(session));
  } else {
    StartSession(std::make_unique<ScissorsTcpSession>(this, client));
  }
}

void Scissors::OnReceivedFrom(const DatagramPtr& datagram) {
  madoka::concurrent::LockGuard lock(&lock_);

  if (stopped_)
    return;

  UdpSession* udp_session = nullptr;

  sockaddr_storage key;
  memset(&key, 0, sizeof(key));
  memmove(&key, &datagram->from, datagram->from_length);

  auto found = udp_sessions_.find(key);
  if (found == udp_sessions_.end()) {
    LOG(INFO) << this << " session not found, creating new one";

    std::unique_ptr<UdpSession> session;

    if (config_->remote_udp()) {
      session.reset(new ScissorsUdpSession(this, datagram->socket));
    } else {
      auto wrapper = std::make_unique<ScissorsWrappingSession>(this);
      if (wrapper == nullptr)
        return;

      wrapper->SetSource(datagram->socket);
      wrapper->SetSourceAddress(datagram->from, datagram->from_length);

      session = std::move(wrapper);
    }

    udp_session = session.get();
    if (StartSession(std::move(session)))
      udp_sessions_.insert({ key, udp_session });
    else
      udp_session = nullptr;
  } else {
    DLOG(INFO) << this << " session found";

    udp_session = found->second;
  }

  if (udp_session != nullptr)
    udp_session->OnReceived(datagram);
}

void Scissors::OnError(DWORD error) {
}

void Scissors::OnConnected(AsyncSocket* socket, HRESULT result,
                           const addrinfo* end_point) {
  if (FAILED(result)) {
    if (result != E_ABORT && end_point->ai_next != nullptr) {
      socket->ConnectAsync(end_point->ai_next, this);
      return;
    }

    LOG(ERROR) << "failed to connect to "
                << config_->remote_address() << ":" << config_->remote_port()
                << " (error: 0x" << std::hex << result << ")";
  }

  lock_.Lock();

  auto listener = connecting_[socket];
  connecting_.erase(socket);
  if (connecting_.empty())
    not_connecting_.WakeAll();

  lock_.Unlock();

  listener->OnConnected(socket, result, end_point);
}
