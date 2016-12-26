// Copyright (c) 2013 dacci.org

#include "service/scissors/scissors_tcp_session.h"

#include <base/logging.h>

#include <memory>

#include "io/net/socket_channel.h"
#include "misc/tunneling_service.h"

namespace juno {
namespace service {
namespace scissors {

ScissorsTcpSession::ScissorsTcpSession(Scissors* service,
                                       const io::ChannelPtr& source)
    : Session(service), source_(source) {
  DLOG(INFO) << this << " session created";
}

ScissorsTcpSession::~ScissorsTcpSession() {
  Stop();

  DLOG(INFO) << this << " session destroyed";
}

bool ScissorsTcpSession::Start() {
  auto socket = std::make_shared<io::net::SocketChannel>();
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

void ScissorsTcpSession::OnConnected(io::net::SocketChannel* /*channel*/,
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

    if (!misc::TunnelingService::Bind(source_, sink_)) {
      LOG(ERROR) << this << " failed to bind channels";
      break;
    }

    source_.reset();
    sink_.reset();
  } while (false);

  service_->EndSession(this);
}

}  // namespace scissors
}  // namespace service
}  // namespace juno
