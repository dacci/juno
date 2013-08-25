// Copyright (c) 2013 dacci.org

#include "net/scissors/scissors_session.h"

#include <assert.h>

#include "misc/string_util.h"
#include "misc/schannel/schannel_context.h"
#include "net/scissors/scissors.h"
#include "net/tunneling_service.h"

ScissorsSession::ScissorsSession(Scissors* service, AsyncSocket* client)
    : service_(service), client_(client),
      remote_(), context_() {
}

ScissorsSession::~ScissorsSession() {
  if (client_ != NULL) {
    delete client_;
    client_ = NULL;
  }

  if (remote_ != NULL) {
    delete remote_;
    remote_ = NULL;
  }

  if (context_ != NULL) {
    delete context_;
    context_ = NULL;
  }
}

bool ScissorsSession::Start() {
  remote_ = new AsyncSocket();
  if (remote_ == NULL)
    return false;

  if (service_->remote_ssl_) {
    std::wstring target_name = ::to_wstring(service_->remote_address_);
    context_ = new SchannelContext(service_->credential_, target_name.c_str());
    if (context_ == NULL)
      return false;
  }

  if (!remote_->ConnectAsync(*service_->resolver_, this))
    return false;

  return true;
}

void ScissorsSession::OnConnected(AsyncSocket* socket, DWORD error) {
  if (error != 0) {
    delete this;
    return;
  }

  if (!service_->remote_ssl_) {
    if (TunnelingService::Bind(client_, remote_)) {
      client_ = NULL;
      remote_ = NULL;
    }

    delete this;
    return;
  }

  // TODO(dacci): initiate SSL hand-shake
}

void ScissorsSession::OnReceived(AsyncSocket* socket, DWORD error, int length) {
  if (socket == client_)
    OnClientReceived(error, length);
  else if (socket == remote_)
    OnRemoteReceived(error, length);
  else
    assert(false);
}

void ScissorsSession::OnSent(AsyncSocket* socket, DWORD error, int length) {
  if (socket == client_)
    OnClientSent(error, length);
  else if (socket == remote_)
    OnRemoteSent(error, length);
  else
    assert(false);
}

bool ScissorsSession::SendAsync(AsyncSocket* socket, const SecBuffer& buffer) {
  return socket->SendAsync(buffer.pvBuffer, buffer.cbBuffer, 0, this);
}

void ScissorsSession::OnClientReceived(DWORD error, int length) {
  if (error != 0 || length == 0) {
    delete this;
    return;
  }
}

void ScissorsSession::OnRemoteReceived(DWORD error, int length) {
  if (error != 0 || length == 0) {
    delete this;
    return;
  }
}

void ScissorsSession::OnClientSent(DWORD error, int length) {
}

void ScissorsSession::OnRemoteSent(DWORD error, int length) {
}
