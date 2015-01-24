// Copyright (c) 2013 dacci.org

#include "service/scissors/scissors_tcp_session.h"

#include <base/logging.h>

#include <memory>

#include "net/channel.h"
#include "net/tunneling_service.h"

using ::madoka::net::AsyncSocket;

ScissorsTcpSession::ScissorsTcpSession(Scissors* service,
                                       const Service::ChannelPtr& source)
    : Session(service), source_(source) {
  DLOG(INFO) << this << " session created";
}

ScissorsTcpSession::~ScissorsTcpSession() {
  Stop();

  DLOG(INFO) << this << " session destroyed";
}

bool ScissorsTcpSession::Start() {
  auto socket = std::make_shared<AsyncSocket>();
  if (socket == nullptr) {
    LOG(ERROR) << this << " failed to create socket";
    return false;
  }

  sink_ = service_->CreateChannel(socket);
  if (sink_ == nullptr) {
    LOG(ERROR) << this << " failed to create sink";
    return false;
  }

  service_->ConnectSocket(socket.get(), this);

  DLOG(INFO) << this << " session started";

  return true;
}

void ScissorsTcpSession::Stop() {
  DLOG(INFO) << this << " stop requested";

  if (sink_ != nullptr)
    sink_->Close();

  if (source_ != nullptr)
    source_->Close();
}

void ScissorsTcpSession::OnConnected(AsyncSocket* socket, DWORD error) {
  if (error == 0) {
    if (TunnelingService::Bind(source_, sink_)) {
      source_.reset();
      sink_.reset();
    } else {
      LOG(ERROR) << this << " failed to bind channels";
    }
  } else {
    LOG(ERROR) << this << " failed to connect: " << error;
  }

  service_->EndSession(this);
}
