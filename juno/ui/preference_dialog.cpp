// Copyright (c) 2013 dacci.org

#include "ui/preference_dialog.h"

#include <wincrypt.h>

#include <utility>

#include "res/resource.h"
#include "ui/servers_page.h"
#include "ui/services_page.h"

PreferenceDialog::PreferenceDialog()
    : CPropertySheetImpl(ID_FILE_NEW), centered_() {
  m_psh.dwFlags |= PSH_NOAPPLYNOW | PSH_NOCONTEXTHELP;

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
  key->QueryStringValue("RemoteProxyUser",
                        proxy_entry->remote_proxy_user.GetBuffer(length),
                        &length);
  proxy_entry->remote_proxy_user.ReleaseBuffer(length);

  length = 0;
  LONG result = key->QueryBinaryValue("RemoteProxyPassword", NULL, &length);
  if (result == ERROR_SUCCESS) {
    DATA_BLOB encrypted = { length, new BYTE[length] }, decrypted = {};
    key->QueryBinaryValue("RemoteProxyPassword", encrypted.pbData, &length);

    result = ::CryptUnprotectData(&encrypted, NULL, NULL, NULL, NULL, 0,
                                  &decrypted);
    if (result != FALSE) {
      proxy_entry->remote_proxy_password.SetString(
          reinterpret_cast<const char*>(decrypted.pbData), decrypted.cbData);
      ::LocalFree(decrypted.pbData);
    }

    delete[] encrypted.pbData;
  }

  CRegKey filters_key;
  if (filters_key.Open(*key, "HeaderFilters") == ERROR_SUCCESS) {
    for (DWORD i = 0; ; ++i) {
      length = 8;
      CString name;
      int result = filters_key.EnumKey(i, name.GetBuffer(length), &length);
      if (result != ERROR_SUCCESS)
        break;

      name.ReleaseBuffer(length);

      CRegKey filter_key;
      filter_key.Open(filters_key, name);

      proxy_entry->header_filters.push_back(HttpHeaderFilter());
      HttpHeaderFilter& filter = proxy_entry->header_filters.back();
      filter.added = false;
      filter.removed = false;

      filter_key.QueryDWORDValue("Request", filter.request);
      filter_key.QueryDWORDValue("Response", filter.response);
      filter_key.QueryDWORDValue("Action", filter.action);

      length = 256;
      filter_key.QueryStringValue("Name", filter.name.GetBuffer(length),
                                  &length);
      filter.name.ReleaseBuffer(length);

      length = 256;
      filter_key.QueryStringValue("Value", filter.value.GetBuffer(length),
                                  &length);
      filter.value.ReleaseBuffer(length);

      length = 256;
      filter_key.QueryStringValue("Replace", filter.replace.GetBuffer(length),
                                  &length);
      filter.replace.ReleaseBuffer(length);
    }
  }
}

void PreferenceDialog::SaveHttpProxy(CRegKey* key, ServiceEntry* entry) {
  HttpProxyEntry* proxy_entry = static_cast<HttpProxyEntry*>(entry->extra);

  key->SetDWORDValue("UseRemoteProxy", proxy_entry->use_remote_proxy);
  key->SetStringValue("RemoteProxyHost", proxy_entry->remote_proxy_host);
  key->SetDWORDValue("RemoteProxyPort", proxy_entry->remote_proxy_port);
  key->SetDWORDValue("AuthRemoteProxy", proxy_entry->auth_remote_proxy);
  key->SetStringValue("RemoteProxyUser", proxy_entry->remote_proxy_user);

  if (!proxy_entry->remote_proxy_password.IsEmpty()) {
    DATA_BLOB decrypted = {
      proxy_entry->remote_proxy_password.GetLength(),
      const_cast<BYTE*>(reinterpret_cast<const BYTE*>(
          proxy_entry->remote_proxy_password.GetString()))
    };
    DATA_BLOB encrypted = {};

    if (::CryptProtectData(&decrypted, NULL, NULL, NULL, NULL, 0, &encrypted)) {
      key->SetBinaryValue("RemoteProxyPassword", encrypted.pbData,
                          encrypted.cbData);
      ::LocalFree(encrypted.pbData);
    }
  }

  CRegKey filters_key;
  filters_key.Create(*key, "HeaderFilters");
  int index = 0;

  for (auto i = proxy_entry->header_filters.begin(),
            l = proxy_entry->header_filters.end(); i != l; ++i) {
    HttpHeaderFilter& filter = *i;

    CString name;
    name.Format("%d", index++);

    CRegKey filter_key;
    filter_key.Create(filters_key, name);

    filter_key.SetDWORDValue("Request", filter.request);
    filter_key.SetDWORDValue("Response", filter.response);
    filter_key.SetDWORDValue("Action", filter.action);
    filter_key.SetStringValue("Name", filter.name);
    filter_key.SetStringValue("Value", filter.value);
    filter_key.SetStringValue("Replace", filter.replace);
  }
}

void PreferenceDialog::LoadSocksProxy(CRegKey* key, ServiceEntry* entry) {
  // currently, no extra config
}

void PreferenceDialog::SaveSocksProxy(CRegKey* key, ServiceEntry* entry) {
  // currently, no extra config
}

void PreferenceDialog::OnShowWindow(BOOL show, UINT status) {
  SetMsgHandled(FALSE);

  if (show && !centered_) {
    centered_ = true;
    CenterWindow();
  }
}
