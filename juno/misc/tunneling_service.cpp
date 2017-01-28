// Copyright (c) 2013 dacci.org

#include "misc/tunneling_service.h"

#include <base/logging.h>

#include "io/channel.h"

namespace juno {
namespace misc {

using ::juno::io::Channel;

TunnelingService* TunnelingService::instance_ = nullptr;

class TunnelingService::Session : public Channel::Listener {
 public:
  Session(TunnelingService* service, const std::shared_ptr<Channel>& from,
          const std::shared_ptr<Channel>& to);
  ~Session();

  HRESULT Start();

  void OnRead(Channel* channel, HRESULT result, void* buffer,
              int length) override;
  void OnWritten(Channel* channel, HRESULT result, void* buffer,
                 int length) override;

  TunnelingService* service_;
  std::shared_ptr<Channel> from_;
  std::shared_ptr<Channel> to_;
  char buffer_[65535];

 private:
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
};

HRESULT TunnelingService::Init() {
  Term();

  instance_ = new TunnelingService();
  if (instance_ == nullptr)
    return E_OUTOFMEMORY;

  return S_OK;
}

void TunnelingService::Term() {
  if (instance_ != nullptr) {
    delete instance_;
    instance_ = nullptr;
  }
}

bool TunnelingService::Bind(const std::shared_ptr<Channel>& a,
                            const std::shared_ptr<Channel>& b) {
  if (instance_ == nullptr)
    return false;

  base::AutoLock guard(instance_->lock_);

  if (instance_->stopped_)
    return false;

  return instance_->BindSocket(a, b) && instance_->BindSocket(b, a);
}

TunnelingService::TunnelingService() : empty_(&lock_), stopped_() {}

TunnelingService::~TunnelingService() {
  base::AutoLock guard(lock_);

  stopped_ = true;

  for (auto& session : sessions_) {
    session->from_->Close();
    session->to_->Close();
  }

  while (!sessions_.empty())
    empty_.Wait();
}

bool TunnelingService::BindSocket(const std::shared_ptr<Channel>& from,
                                  const std::shared_ptr<Channel>& to) {
  CHECK(!stopped_);
  lock_.AssertAcquired();

  auto session = std::make_unique<Session>(this, from, to);
  if (session == nullptr)
    return false;

  if (FAILED(session->Start()))
    return false;

  sessions_.push_back(std::move(session));

  return true;
}

void TunnelingService::EndSession(Session* session) {
  auto pair = new ServiceSessionPair(this, session);
  if (pair == nullptr ||
      !TrySubmitThreadpoolCallback(EndSessionImpl, pair, nullptr)) {
    delete pair;
    EndSessionImpl(session);
  }
}

void CALLBACK TunnelingService::EndSessionImpl(
    PTP_CALLBACK_INSTANCE /*instance*/, void* param) {
  auto pair = static_cast<ServiceSessionPair*>(param);
  pair->first->EndSessionImpl(pair->second);
  delete pair;
}

void TunnelingService::EndSessionImpl(Session* session) {
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

  lock_.Release();

  removed.reset();
}

TunnelingService::Session::Session(TunnelingService* service,
                                   const std::shared_ptr<Channel>& from,
                                   const std::shared_ptr<Channel>& to)
    : service_(service), from_(from), to_(to) {}

TunnelingService::Session::~Session() {
  from_->Close();
  to_->Close();
}

HRESULT TunnelingService::Session::Start() {
  return from_->ReadAsync(buffer_, sizeof(buffer_), this);
}

void TunnelingService::Session::OnRead(Channel* /*channel*/, HRESULT result,
                                       void* buffer, int length) {
  if (SUCCEEDED(result) && length > 0) {
    result = to_->WriteAsync(buffer, length, this);
    if (SUCCEEDED(result))
      return;
  }

  service_->EndSession(this);
}

void TunnelingService::Session::OnWritten(Channel* /*channel*/, HRESULT result,
                                          void* /*buffer*/, int length) {
  if (SUCCEEDED(result) && length > 0) {
    result = from_->ReadAsync(buffer_, sizeof(buffer_), this);
    if (SUCCEEDED(result))
      return;
  }

  service_->EndSession(this);
}

}  // namespace misc
}  // namespace juno
