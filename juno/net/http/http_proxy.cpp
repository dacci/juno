// Copyright (c) 2013 dacci.org

#include "net/http/http_proxy.h"

HttpProxy::HttpProxy(const char* address, const char* port)
    : address_(address), port_(port) {
}

HttpProxy::~HttpProxy() {
  Stop();
}

bool HttpProxy::Start() {
  resolver_.ai_flags = AI_PASSIVE;
  resolver_.ai_socktype = SOCK_STREAM;
  if (!resolver_.Resolve(address_.c_str(), port_.c_str()))
    return false;

  bool started = false;

  for (auto i = resolver_.begin(), l = resolver_.end(); i != l; ++i) {
    AsyncServerSocket* server = new AsyncServerSocket();
    if (server->Bind(*i) && server->Listen(SOMAXCONN) &&
        server->AcceptAsync(this)) {
      started = true;
      servers_.push_back(server);
    }
  }

  return started;
}

void HttpProxy::Stop() {
  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i) {
    (*i)->Close();
    delete *i;
  }
  servers_.clear();
}

void HttpProxy::OnAccepted(AsyncServerSocket* server, AsyncSocket* client,
                           DWORD error) {
  if (error == 0) {
    server->AcceptAsync(this);

    HttpProxySession* session = new HttpProxySession(client);
    if (!session->Start())
      delete session;
  } else {
    delete client;
  }
}
