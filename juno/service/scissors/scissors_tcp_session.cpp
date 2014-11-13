// Copyright (c) 2013 dacci.org

#include "service/scissors/scissors_tcp_session.h"

#include "net/secure_socket_channel.h"
#include "net/socket_channel.h"
#include "net/tunneling_service.h"
#include "service/scissors/scissors_config.h"

using ::madoka::net::AsyncSocket;

ScissorsTcpSession::ScissorsTcpSession(Scissors* service,
                                       const Service::ChannelPtr& client)
    : Session(service), client_(client), remote_(new AsyncSocket()) {
  resolver_.SetType(SOCK_STREAM);
}

ScissorsTcpSession::~ScissorsTcpSession() {
  Stop();
}

bool ScissorsTcpSession::Start() {
  if (remote_ == nullptr)
    return false;

  if (!resolver_.Resolve(service_->config_->remote_address().c_str(),
                         service_->config_->remote_port()))
    return false;

  remote_->ConnectAsync(*resolver_.begin(), this);

  return true;
}

void ScissorsTcpSession::Stop() {
  if (client_ != nullptr)
    client_->Close();

  if (remote_ != nullptr) {
    if (remote_->connected())
      remote_->Close();
    else
      remote_->CancelAsyncConnect();
  }
}

void ScissorsTcpSession::OnConnected(AsyncSocket* socket, DWORD error) {
  if (error == 0) {
    TunnelingService::ChannelPtr remote_channel;

    if (service_->config_->remote_ssl()) {
      auto channel = std::make_shared<SecureSocketChannel>(
          service_->credential_.get(), remote_, false);
      if (channel != nullptr)
        channel->context()->set_target_name(
          service_->config_->remote_address());

      remote_channel = std::move(channel);
    } else {
      remote_channel = std::make_shared<SocketChannel>(remote_);
    }

    if (remote_channel != nullptr &&
        TunnelingService::Bind(client_, remote_channel)) {
      client_.reset();
      remote_.reset();
    }
  }

  service_->EndSession(this);
}
