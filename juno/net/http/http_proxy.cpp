// Copyright (c) 2013 dacci.org

#include "net/http/http_proxy.h"

#include <wincrypt.h>

#include <madoka/concurrent/lock_guard.h>

#include <utility>

#include "misc/registry_key-inl.h"
#include "net/async_server_socket.h"
#include "net/http/http_proxy_session.h"

HttpProxy::HttpProxy() : stopped_(), use_remote_proxy_(), remote_proxy_port_() {
  empty_event_ = ::CreateEvent(NULL, TRUE, TRUE, NULL);
}

HttpProxy::~HttpProxy() {
  Stop();
  ::CloseHandle(empty_event_);
}

bool HttpProxy::Setup(HKEY key) {
  RegistryKey reg_key(key);

  use_remote_proxy_ = reg_key.QueryInteger("UseRemoteProxy");

  if (!reg_key.QueryString("RemoteProxyHost", &remote_proxy_host_))
    use_remote_proxy_ = FALSE;

  remote_proxy_port_ = reg_key.QueryInteger("RemoteProxyPort");
  if (remote_proxy_port_ <= 0 || 65536 <= remote_proxy_port_)
    use_remote_proxy_ = FALSE;

  auth_remote_proxy_ = reg_key.QueryInteger("AuthRemoteProxy");

  if (!reg_key.QueryString("RemoteProxyUser", &remote_proxy_user_))
    auth_remote_proxy_ = 0;

  int length = 0;
  if (reg_key.QueryBinary("RemoteProxyPassword", NULL, &length)) {
    BYTE* buffer = new BYTE[length];
    reg_key.QueryBinary("RemoteProxyPassword", buffer, &length);

    DATA_BLOB encrypted = { length, buffer }, decrypted = {};
    if (::CryptUnprotectData(&encrypted, NULL, NULL, NULL, NULL, 0,
                             &decrypted)) {
      remote_proxy_password_.assign(reinterpret_cast<char*>(decrypted.pbData),
                                    decrypted.cbData);
      ::LocalFree(decrypted.pbData);
    }

    delete[] buffer;
  }

  std::string auth = remote_proxy_user_ + ':' + remote_proxy_password_;
  DWORD buffer_size = (((auth.size() - 1) / 3) + 2) * 4;
  char* buffer = new char[buffer_size];

  ::CryptBinaryToStringA(reinterpret_cast<const BYTE*>(auth.c_str()),
                         auth.size(), CRYPT_STRING_BASE64, buffer,
                         &buffer_size);
  char* end = buffer + buffer_size - 1;
  while (end > buffer) {
    if (!::isspace(*end))
      break;
    --end;
  }

  basic_authorization_ = "Basic ";
  basic_authorization_.append(buffer, end + 1);

  delete[] buffer;

  RegistryKey filters_key;
  if (filters_key.Open(reg_key, "HeaderFilters")) {
    for (DWORD i = 0; ; ++i) {
      std::string name;
      if (!filters_key.EnumerateKey(i, &name))
        break;

      RegistryKey filter_key;
      filter_key.Open(filters_key, name);

      header_filters_.push_back(HeaderFilter());
      HeaderFilter& filter = header_filters_.back();

      filter.request = filter_key.QueryInteger("Request") != 0;
      filter.response = filter_key.QueryInteger("Response") != 0;
      filter.action =
          static_cast<FilterAction>(filter_key.QueryInteger("Action"));
      filter_key.QueryString("Name", &filter.name);
      filter_key.QueryString("Value", &filter.value);
      filter_key.QueryString("Replace", &filter.replace);
    }

    filters_key.Close();
  }

  reg_key.Detach();

  return true;
}

void HttpProxy::Stop() {
  madoka::concurrent::LockGuard lock(&critical_section_);

  stopped_ = true;

  for (auto i = sessions_.begin(), l = sessions_.end(); i != l; ++i)
    (*i)->Stop();

  while (true) {
    critical_section_.Unlock();
    ::WaitForSingleObject(empty_event_, INFINITE);
    critical_section_.Lock();

    if (sessions_.empty())
      break;
  }
}

void HttpProxy::FilterHeaders(HttpHeaders* headers, bool request) {
  for (auto i = header_filters_.begin(), l = header_filters_.end();
       i != l; ++i) {
    HeaderFilter& filter = *i;

    if (request && filter.request || !request && filter.response) {
      switch (filter.action) {
        case Set:
          headers->SetHeader(filter.name, filter.value);
          break;

        case Append:
          headers->AppendHeader(filter.name, filter.value);
          break;

        case Add:
          headers->AddHeader(filter.name, filter.value);
          break;

        case Unset:
          headers->RemoveHeader(filter.name);
          break;

        case Merge:
          headers->MergeHeader(filter.name, filter.value);
          break;

        case Edit:
          headers->EditHeader(filter.name, filter.value, filter.replace, false);
          break;

        case EditR:
          headers->EditHeader(filter.name, filter.value, filter.replace, true);
          break;
      }
    }
  }
}

void HttpProxy::EndSession(HttpProxySession* session) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  sessions_.remove(session);
  if (sessions_.empty())
    ::SetEvent(empty_event_);
}

bool HttpProxy::OnAccepted(AsyncSocket* client) {
  madoka::concurrent::LockGuard lock(&critical_section_);

  if (stopped_)
    return false;

  HttpProxySession* session = new HttpProxySession(this);
  if (session == NULL)
    return false;

  sessions_.push_back(session);
  ::ResetEvent(empty_event_);

  if (!session->Start(client)) {
    delete session;
    return false;
  }

  return true;
}

void HttpProxy::OnError(DWORD error) {
}
