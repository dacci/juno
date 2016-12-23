// Copyright (c) 2013 dacci.org

#include "service/scissors/scissors_tcp_session.h"

#include <base/logging.h>

#include <memory>

#include "net/socket_channel.h"
#include "net/tunneling_service.h"

ScissorsTcpSession::ScissorsTcpSession(Scissors* service,
                                       const ChannelPtr& source)
    : Session(service), source_(source) {
  DLOG(INFO) << this << " session created";
}

ScissorsTcpSession::~ScissorsTcpSession() {
  Stop();

  DLOG(INFO) << this << " session destroyed";
}

bool ScissorsTcpSession::Start() {
  auto socket = std::make_shared<SocketChannel>();
  if (socket == nullptr) {
    LOG(ERROR) << this << " failed to create socket";
    return false;
  }

  sink_ = socket;

  auto result = service_->ConnectSocket(socket.get(), this);
  if (FAILED(result)) {
    LOG(ERROR) << this << " failed to connect: 0x" << std::hex << result;
    return false;
  }

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

void ScissorsTcpSession::OnConnected(SocketChannel* /*channel*/,
                                     HRESULT result) {
  do {
    if (FAILED(result)) {
      LOG(ERROR) << this << " failed to connect: 0x" << std::hex << result;
      break;
    }

    sink_ = service_->CreateChannel(sink_);
    if (sink_ == nullptr) {
      LOG(ERROR) << this << " failed to customize channel.";
      break;
    }

    if (!TunnelingService::Bind(source_, sink_)) {
      LOG(ERROR) << this << " failed to bind channels";
      break;
    }

    source_.reset();
    sink_.reset();
  } while (false);

  service_->EndSession(this);
}
