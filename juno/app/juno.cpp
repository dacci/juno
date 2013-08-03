// Copyright (c) 2013 dacci.org

#include <crtdbg.h>
#include <stdint.h>

#include <atlbase.h>
#include <atlstr.h>

#include <madoka/net/winsock.h>

#include <map>
#include <string>
#include <vector>

#include "net/http/http_proxy.h"
#include "net/tcp_server.h"

typedef std::map<std::string, ServiceProvider*> ServiceMap;

ServiceProvider* LoadService(HKEY parent, const char* key_name) {
  CRegKey reg_key;
  LONG result = reg_key.Open(parent, key_name);
  if (result != ERROR_SUCCESS)
    return NULL;

  CString provider_name;
  ULONG length = 64;
  result = reg_key.QueryStringValue("Provider", provider_name.GetBuffer(length),
                                    &length);
  if (result != ERROR_SUCCESS)
    return NULL;
  provider_name.ReleaseBuffer(length);

  ServiceProvider* provider = NULL;

  if (provider_name.Compare("HttpProxy") == 0)
    provider = new HttpProxy();

  if (provider == NULL)
    return NULL;

  if (!provider->Setup(reg_key)) {
    delete provider;
    return NULL;
  }

  return provider;
}

TcpServer* LoadServer(const ServiceMap& services, HKEY parent,
                      const char* key_name) {
  CRegKey reg_key;
  LONG result = reg_key.Open(parent, key_name);
  if (result != ERROR_SUCCESS)
    return NULL;

  DWORD enabled;
  result = reg_key.QueryDWORDValue("Enabled", enabled);
  if (result == ERROR_SUCCESS && !enabled)
    return NULL;

  CString service;
  ULONG length = 64;
  result = reg_key.QueryStringValue("Service", service.GetBuffer(length),
                                    &length);
  if (result != ERROR_SUCCESS)
    return NULL;

  auto service_entry = services.find(service.GetString());
  if (service_entry == services.end())
    return NULL;

  DWORD listen;
  result = reg_key.QueryDWORDValue("Listen", listen);
  if (result != ERROR_SUCCESS)
    return NULL;

  CString bind;
  length = 48;
  result = reg_key.QueryStringValue("Bind", bind.GetBuffer(length), &length);
  if (result != ERROR_SUCCESS)
    return NULL;
  bind.ReleaseBuffer(length);

  TcpServer* server = new TcpServer();
  if (server == NULL)
    return NULL;

  if (!server->Setup(bind, listen)) {
    delete server;
    return NULL;
  }

  server->SetService(service_entry->second);

  return server;
}

int main(int argc, char* argv[]) {
#ifdef _DEBUG
  {
    int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    flags |= _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF;
    _CrtSetDbgFlag(flags);
  }
#endif  // _DEBUG

  madoka::net::WinSock winsock(WINSOCK_VERSION);
  if (!winsock.Initialized())
    return __LINE__;

  CRegKey app_key;
  LONG result = app_key.Create(HKEY_CURRENT_USER, "Software\\dacci.org\\Juno");
  if (result != ERROR_SUCCESS)
    return __LINE__;

  ServiceMap services;
  std::vector<TcpServer*> servers;

  do {
    CRegKey services_key;
    result = services_key.Create(app_key, "Services");
    if (result != ERROR_SUCCESS)
      break;

    for (DWORD i = 0; ; ++i) {
      CString name;
      DWORD name_length = 64;
      result = services_key.EnumKey(i, name.GetBuffer(name_length),
                                    &name_length);
      if (result != ERROR_SUCCESS)
        break;
      name.ReleaseBuffer(name_length);

      ServiceProvider* service = LoadService(services_key, name);
      if (service != NULL)
        services[name.GetString()] = service;
    }

    CRegKey servers_key;
    result = servers_key.Create(app_key, "Servers");
    if (result != ERROR_SUCCESS)
      break;

    for (DWORD i = 0; ; ++i) {
      CString name;
      DWORD name_length = 64;
      result = servers_key.EnumKey(i, name.GetBuffer(name_length),
                                   &name_length);
      if (result != ERROR_SUCCESS)
        break;
      name.ReleaseBuffer(name_length);

      TcpServer* server = LoadServer(services, servers_key, name);
      if (server != NULL)
        servers.push_back(server);
    }
  } while (false);

  if (!servers.empty()) {
    for (auto i = servers.begin(), l = servers.end(); i != l; ++i)
      (*i)->Start();

    ::getchar();
  }

  for (auto i = servers.begin(), l = servers.end(); i != l; ++i)
    (*i)->Stop();

  for (auto i = services.begin(), l = services.end(); i != l; ++i)
    i->second->Stop();

  for (auto i = servers.begin(), l = servers.end(); i != l; ++i)
    delete *i;

  for (auto i = services.begin(), l = services.end(); i != l; ++i)
    delete i->second;

  return 0;
}
