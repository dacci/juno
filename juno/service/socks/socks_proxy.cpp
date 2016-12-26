// Copyright (c) 2013 dacci.org

#include "service/socks/socks_proxy.h"

#include <base/logging.h>

#include "service/socks/socks_session_4.h"
#include "service/socks/socks_session_5.h"

namespace juno {
namespace service {
namespace socks {

SocksProxy::SocksProxy() : empty_(&lock_), stopped_() {}

SocksProxy::~SocksProxy() {}

void SocksProxy::Stop() {
  base::AutoLock guard(lock_);

  stopped_ = true;

  for (auto& candidate : candidates_)
    candidate->Close();

  for (auto& session : sessions_)
    session->Stop();

  WaitEmpty();
}

void SocksProxy::EndSession(SocksSession* session) {
  auto pair = new ServiceSessionPair(this, session);
  if (pair == nullptr ||
      !TrySubmitThreadpoolCallback(EndSessionImpl, pair, nullptr)) {
    delete pair;
    EndSessionImpl(session);
  }
}

void SocksProxy::OnAccepted(const io::ChannelPtr& client) {
  base::AutoLock guard(lock_);

  if (stopped_)
    return;

  auto buffer = std::make_unique<char[]>(kBufferSize);
  if (buffer == nullptr) {
    LOG(ERROR) << "Failed to allocate buffer.";
    return;
  }

  auto result = client->ReadAsync(buffer.get(), kBufferSize, this);
  if (SUCCEEDED(result)) {
    candidates_.push_back(client);
    buffer.release();
  } else {
    LOG(ERROR) << "Failed to receive: 0x" << std::hex << result;
  }
}

void SocksProxy::CheckEmpty() {
  if (candidates_.empty() && sessions_.empty())
    empty_.Broadcast();
}

void SocksProxy::WaitEmpty() {
  while (!candidates_.empty() || !sessions_.empty())
    empty_.Wait();
}

void SocksProxy::OnRead(io::Channel* channel, HRESULT result, void* buffer,
                        int length) {
  std::unique_ptr<char[]> message(static_cast<char*>(buffer));
  io::ChannelPtr candidate;

  base::AutoLock guard(lock_);

  for (auto i = candidates_.begin(), l = candidates_.end(); i != l; ++i) {
    if (i->get() == channel) {
      candidate = std::move(*i);
      candidates_.erase(i);
      CheckEmpty();
      break;
    }
  }
  CHECK(candidate != nullptr) << "Candidate not found.";

  if (stopped_) {
    LOG(WARNING) << "Service stopped.";
    return;
  }

  if (FAILED(result)) {
    LOG(ERROR) << "Failed to receive: 0x" << std::hex << result;
    return;
  }

  if (length < 1) {
    LOG(WARNING) << "Client disconnected.";
    return;
  }

  std::unique_ptr<SocksSession> session;

  switch (message[0]) {
    case 4:
      session = std::make_unique<SocksSession4>(this, std::move(candidate));
      break;

    case 5:
      session = std::make_unique<SocksSession5>(this, std::move(candidate));
      break;

    default:
      LOG(ERROR) << "Unknown protocol version: "
                 << static_cast<int>(message[0]);
      return;
  }

  if (session == nullptr) {
    LOG(ERROR) << "Failed to create session.";
    return;
  }

  result = session->Start(std::move(message), length);
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to start session: 0x" << std::hex << result;
    return;
  }

  sessions_.push_back(std::move(session));
}

void SocksProxy::OnWritten(io::Channel* /*channel*/, HRESULT /*result*/,
                           void* /*buffer*/, int /*length*/) {
  DLOG(FATAL) << "It's not intended to be used like this.";
}

void CALLBACK SocksProxy::EndSessionImpl(PTP_CALLBACK_INSTANCE /*instance*/,
                                         void* param) {
  auto pair = static_cast<ServiceSessionPair*>(param);
  pair->first->EndSessionImpl(pair->second);
  delete pair;
}

void SocksProxy::EndSessionImpl(SocksSession* session) {
  std::unique_ptr<SocksSession> removed;
  base::AutoLock guard(lock_);

  for (auto i = sessions_.begin(), l = sessions_.end(); i != l; ++i) {
    if (i->get() == session) {
      removed = std::move(*i);
      sessions_.erase(i);
      CheckEmpty();
      return;
    }
  }

  DLOG(WARNING) << "Session not found.";
}

}  // namespace socks
}  // namespace service
}  // namespace juno
