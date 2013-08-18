// Copyright (c) 2013 dacci.org

#ifndef JUNO_UI_PREFERENCE_DIALOG_H_
#define JUNO_UI_PREFERENCE_DIALOG_H_

#include <atlbase.h>

#include <atlapp.h>
#include <atlcrack.h>
#include <atldlgs.h>

#include <utility>
#include <vector>

class PreferenceDialog : public CPropertySheetImpl<PreferenceDialog> {
 public:
  struct ServiceExtra {
    virtual ~ServiceExtra() {
    }
  };

  struct ServiceEntry {
    ServiceEntry() : extra(NULL) {
    }

    ServiceEntry(ServiceEntry&& other) : extra(NULL) {
      *this = std::move(other);
    }

    ~ServiceEntry() {
      if (extra != NULL)
        delete extra;
    }

    ServiceEntry& operator=(ServiceEntry&& other) {
      if (&other != this) {
        name = other.name;
        provider = other.provider;
        extra = other.extra;
        other.extra = NULL;
      }

      return *this;
    }

    CString name;
    CString provider;
    ServiceExtra* extra;
  };

  struct HttpHeaderFilter {
    bool added;
    bool removed;
    DWORD request;
    DWORD response;
    DWORD action;
    CString name;
    CString value;
    CString replace;
  };

  struct HttpProxyEntry : ServiceExtra {
    HttpProxyEntry()
        : use_remote_proxy(0), remote_proxy_port(0), auth_remote_proxy(0) {
    }

    virtual ~HttpProxyEntry() {
    }

    DWORD use_remote_proxy;
    CString remote_proxy_host;
    DWORD remote_proxy_port;
    DWORD auth_remote_proxy;
    CString remote_proxy_user;
    CString remote_proxy_password;
    std::vector<HttpHeaderFilter> header_filters;
  };

  struct ServerEntry {
    ServerEntry() : enabled(TRUE), listen(0) {
    }

    CString name;
    DWORD enabled;
    CString bind;
    DWORD listen;
    CString service;
  };

  PreferenceDialog();
  ~PreferenceDialog();

  BEGIN_MSG_MAP(PreferenceDialog)
    MSG_WM_SHOWWINDOW(OnShowWindow)

    CHAIN_MSG_MAP(CPropertySheetImpl)
  END_MSG_MAP()

  void SaveServices();
  void LoadServices();

  void SaveServers();
  void LoadServers();

  std::vector<ServiceEntry> services_;
  std::vector<ServerEntry> servers_;

 private:
  void LoadHttpProxy(CRegKey* key, ServiceEntry* entry);
  void SaveHttpProxy(CRegKey* key, ServiceEntry* entry);

  void LoadSocksProxy(CRegKey* key, ServiceEntry* entry);
  void SaveSocksProxy(CRegKey* key, ServiceEntry* entry);

  void OnShowWindow(BOOL show, UINT status);

  bool centered_;
};

#endif  // JUNO_UI_PREFERENCE_DIALOG_H_
