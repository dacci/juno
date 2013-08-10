// Copyright (c) 2013 dacci.org

#include "ui/preference_dialog.h"

#include <atlenc.h>

#include <utility>

#include "res/resource.h"
#include "ui/servers_page.h"
#include "ui/services_page.h"

PreferenceDialog::PreferenceDialog() : CPropertySheetImpl(ID_FILE_NEW) {
  LoadServices();
  LoadServers();

  AddPage(*new ServicesPage(this));
  AddPage(*new ServersPage(this));
}

PreferenceDialog::~PreferenceDialog() {
}

void PreferenceDialog::LoadServices() {
  CRegKey services_key;
  services_key.Open(HKEY_CURRENT_USER, "Software\\dacci.org\\Juno\\Services");

  for (DWORD i = 0; ; ++i) {
    ServiceEntry entry;

    DWORD length = 64;
    LONG result = services_key.EnumKey(i, entry.name.GetBuffer(length),
                                       &length);
    if (result != ERROR_SUCCESS)
      break;

    entry.name.ReleaseBuffer(length);

    CRegKey service_key;
    service_key.Open(services_key, entry.name);

    length = 64;
    result = service_key.QueryStringValue("Provider",
                                          entry.provider.GetBuffer(length),
                                          &length);
    ATLASSERT(result == ERROR_SUCCESS);
    if (result != ERROR_SUCCESS)
      continue;

    entry.provider.ReleaseBuffer(length);

    if (entry.provider.Compare("HttpProxy") == 0)
      LoadHttpProxy(&service_key, &entry);
    else if (entry.provider.Compare("SocksProxy") == 0)
      LoadSocksProxy(&service_key, &entry);

    services_.push_back(std::move(entry));
  }
}

void PreferenceDialog::SaveServices() {
  CRegKey app_key;
  app_key.Open(HKEY_CURRENT_USER, "Software\\dacci.org\\Juno");

  ::SHDeleteKey(app_key, "Services");

  CRegKey services_key;
  services_key.Create(app_key, "Services");

  for (auto i = services_.begin(), l = services_.end(); i != l; ++i) {
    CRegKey service_key;
    service_key.Create(services_key, i->name);

    service_key.SetStringValue("Provider", i->provider);

    if (i->provider.Compare("HttpProxy") == 0)
      SaveHttpProxy(&service_key, &*i);
  }
}

void PreferenceDialog::LoadServers() {
  CRegKey servers_key;
  servers_key.Open(HKEY_CURRENT_USER, "Software\\dacci.org\\Juno\\Servers");

  for (DWORD i = 0; ; ++i) {
    ServerEntry entry;

    DWORD length = 64;
    LONG result = servers_key.EnumKey(i, entry.name.GetBuffer(length), &length);
    if (result != ERROR_SUCCESS)
      break;

    entry.name.ReleaseBuffer(length);

    CRegKey server_key;
    server_key.Open(servers_key, entry.name);

    server_key.QueryDWORDValue("Enabled", entry.enabled);

    length = 64;
    result = server_key.QueryStringValue("Bind", entry.bind.GetBuffer(length),
                                         &length);
    entry.bind.ReleaseBuffer(length);

    server_key.QueryDWORDValue("Listen", entry.listen);

    length = 64;
    result = server_key.QueryStringValue("Service",
                                         entry.service.GetBuffer(length),
                                         &length);
    entry.service.ReleaseBuffer(length);

    servers_.push_back(std::move(entry));
  }
}

void PreferenceDialog::SaveServers() {
  CRegKey app_key;
  app_key.Open(HKEY_CURRENT_USER, "Software\\dacci.org\\Juno");

  ::SHDeleteKey(app_key, "Servers");

  CRegKey servers_key;
  servers_key.Create(app_key, "Servers");

  for (auto i = servers_.begin(), l = servers_.end(); i != l; ++i) {
    CRegKey server_key;
    server_key.Create(servers_key, i->name);

    server_key.SetDWORDValue("Enabled", i->enabled);
    server_key.SetStringValue("Bind", i->bind);
    server_key.SetDWORDValue("Listen", i->listen);
    server_key.SetStringValue("Service", i->service);
  }
}

void PreferenceDialog::LoadHttpProxy(CRegKey* key, ServiceEntry* entry) {
  HttpProxyEntry* proxy_entry = new HttpProxyEntry();
  entry->extra = proxy_entry;

  key->QueryDWORDValue("UseRemoteProxy", proxy_entry->use_remote_proxy);

  ULONG length = 256;
  key->QueryStringValue("RemoteProxyHost",
                        proxy_entry->remote_proxy_host.GetBuffer(length),
                        &length);
  proxy_entry->remote_proxy_host.ReleaseBuffer(length);

  key->QueryDWORDValue("RemoteProxyPort", proxy_entry->remote_proxy_port);
  key->QueryDWORDValue("AuthRemoteProxy", proxy_entry->auth_remote_proxy);

  length = 256;
  CString auth;
  key->QueryStringValue("RemoteProxyAuth", auth.GetBuffer(length), &length);
  auth.ReleaseBuffer(length);

  if (length > 0) {
    int decoded_length = ::Base64DecodeGetRequiredLength(length);
    CString decoded;
    ::Base64Decode(auth, auth.GetLength(),
                   reinterpret_cast<BYTE*>(decoded.GetBuffer(decoded_length)),
                   &decoded_length);
    decoded.ReleaseBuffer(decoded_length);

    int index = decoded.Find(':', 0);
    proxy_entry->remote_proxy_user = decoded.Left(index);
    proxy_entry->remote_proxy_password = decoded.Mid(index + 1);
  }
}

void PreferenceDialog::SaveHttpProxy(CRegKey* key, ServiceEntry* entry) {
  HttpProxyEntry* proxy_entry = static_cast<HttpProxyEntry*>(entry->extra);

  key->SetDWORDValue("UseRemoteProxy", proxy_entry->use_remote_proxy);
  key->SetStringValue("RemoteProxyHost", proxy_entry->remote_proxy_host);
  key->SetDWORDValue("RemoteProxyPort", proxy_entry->remote_proxy_port);
  key->SetDWORDValue("AuthRemoteProxy", proxy_entry->auth_remote_proxy);

  CString auth;
  auth.Format("%s:%s", proxy_entry->remote_proxy_user,
              proxy_entry->remote_proxy_password);

  DWORD flags = ATL_BASE64_FLAG_NOCRLF;
  int length = ::Base64EncodeGetRequiredLength(auth.GetLength(), flags);

  CString encoded;
  ::Base64Encode(reinterpret_cast<const BYTE*>(auth.GetString()),
                 auth.GetLength(), encoded.GetBuffer(length), &length, flags);
  encoded.ReleaseBuffer(length);

  key->SetStringValue("RemoteProxyAuth", encoded);
}

void PreferenceDialog::LoadSocksProxy(CRegKey* key, ServiceEntry* entry) {
  // currently, no extra config
}

void PreferenceDialog::SaveSocksProxy(CRegKey* key, ServiceEntry* entry) {
  // currently, no extra config
}
