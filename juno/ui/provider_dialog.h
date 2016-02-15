// Copyright (c) 2013 dacci.org

#ifndef JUNO_UI_PROVIDER_DIALOG_H_
#define JUNO_UI_PROVIDER_DIALOG_H_

#include <atlbase.h>
#include <atlwin.h>

#include <atlapp.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include <atlddx.h>

#include <string>
#include <vector>

#include "res/resource.h"
#include "ui/dialog_impl_ex.h"

class PreferenceDialog;
class ServiceProvider;

class ProviderDialog
    : public DialogImplEx<ProviderDialog>,
      public CWinDataExchange<ProviderDialog> {
 public:
  static const UINT IDD = IDD_PROVIDER;

  explicit ProviderDialog(PreferenceDialog* parent);
  ~ProviderDialog();

  BEGIN_DDX_MAP(ProviderDialog)
    DDX_CONTROL_HANDLE(IDC_NAME, name_edit_)
    DDX_CONTROL_HANDLE(IDC_PROVIDER, provider_combo_)
  END_DDX_MAP()

  BEGIN_MSG_MAP(ProviderDialog)
    MSG_WM_INITDIALOG(OnInitDialog)

    COMMAND_ID_HANDLER_EX(IDOK, OnOk)
    COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)

    CHAIN_MSG_MAP(DialogImplEx)
  END_MSG_MAP()

  const std::string& GetProviderName() const;

  const std::string& name() const {
    return name_;
  }

 private:
  BOOL OnInitDialog(CWindow focus, LPARAM init_param);

  void OnOk(UINT notify_code, int id, CWindow control);
  void OnCancel(UINT notify_code, int id, CWindow control);

  PreferenceDialog* const parent_;

  std::vector<std::string> provider_names_;
  std::string name_;

  CEdit name_edit_;
  CComboBox provider_combo_;
  int provider_index_;
};

#endif  // JUNO_UI_PROVIDER_DIALOG_H_
