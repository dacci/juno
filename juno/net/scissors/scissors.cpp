// Copyright (c) 2013 dacci.org

#include "net/scissors/scissors.h"

#include "misc/registry_key.h"
#include "misc/schannel/schannel_credential.h"
#include "net/scissors/scissors_session.h"

Scissors::Scissors() : remote_port_(), remote_ssl_(), credential_() {
}

Scissors::~Scissors() {
  if (credential_ != NULL)
    delete credential_;
}

bool Scissors::Setup(HKEY key) {
  RegistryKey reg_key(key);

  reg_key.QueryString("RemoteAddress", &remote_address_);
  reg_key.QueryInteger("RemotePort", &remote_port_);
  reg_key.QueryInteger("RemoteSSL", &remote_ssl_);

  reg_key.Detach();

  resolver_.ai_socktype = SOCK_STREAM;
  if (!resolver_.Resolve(remote_address_.c_str(), remote_port_))
    return false;

  if (remote_ssl_) {
    credential_ = new SchannelCredential();
    if (credential_ == NULL)
      return false;

    credential_->SetEnabledProtocols(SP_PROT_SSL3TLS1_X_CLIENTS);
    credential_->SetFlags(SCH_CRED_MANUAL_CRED_VALIDATION);

    SECURITY_STATUS status = credential_->AcquireHandle(SECPKG_CRED_OUTBOUND);
    if (FAILED(status))
      return false;
  }

  return true;
}

void Scissors::Stop() {
}

bool Scissors::OnAccepted(AsyncSocket* client) {
  ScissorsSession* session = new ScissorsSession(this);
  if (session == NULL)
    return false;

  if (!session->Start(client)) {
    delete session;
    return false;
  }

  return true;
}

void Scissors::OnError(DWORD error) {
}
