// Copyright (c) 2013 dacci.org

#include "ui/main_frame.h"

#include <atlstr.h>

#include <base/logging.h>

#include "app/constants.h"
#include "app/service_manager.h"
#include "net/tunneling_service.h"
#include "ui/preference_dialog.h"

const UINT MainFrame::WM_TASKBARCREATED =
    RegisterWindowMessage(_T("TaskbarCreated"));

MainFrame::MainFrame()
    : old_windows_(false), notify_icon_(), configuring_(false) {}

MainFrame::~MainFrame() {}

void MainFrame::TrackTrayMenu(int x, int y) {
  if (configuring_)
    return;

  CMenu menu;
  menu.LoadMenu(IDR_TRAY_MENU);
  auto popup_menu = menu.GetSubMenu(0);
  popup_menu.SetMenuDefaultItem(kDefaultTrayCommand);

  SetForegroundWindow(m_hWnd);
  popup_menu.TrackPopupMenu(TPM_RIGHTBUTTON, x, y, m_hWnd);
  PostMessage(WM_NULL);
}

int MainFrame::OnCreate(CREATESTRUCT* /*create_struct*/) {
  CString message;

  notify_icon_.cbSize = sizeof(notify_icon_);
  notify_icon_.hWnd = m_hWnd;
  notify_icon_.uFlags =
      NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_GUID | NIF_SHOWTIP;
  notify_icon_.uCallbackMessage = WM_TRAYNOTIFY;
  notify_icon_.uVersion = NOTIFYICON_VERSION_4;
  notify_icon_.guidItem = GUID_JUNO_APPLICATION;

  auto result = LoadIconMetric(ModuleHelper::GetResourceInstance(),
                               MAKEINTRESOURCE(IDR_MAIN_FRAME), LIM_SMALL,
                               &notify_icon_.hIcon);
  if (FAILED(result)) {
    LOG(ERROR) << "LoadIconMetric() failed: 0x" << std::hex << result;
    message.LoadString(IDS_ERR_INIT_FAILED);
    MessageBox(message, nullptr, MB_ICONERROR);
    return -1;
  }

  auto length = AtlLoadString(IDR_MAIN_FRAME, notify_icon_.szTip,
                              _countof(notify_icon_.szTip));
  if (length == 0) {
    LOG(ERROR) << "LoadString() failed: " << GetLastError();
    message.LoadString(IDS_ERR_INIT_FAILED);
    MessageBox(message, nullptr, MB_ICONERROR);
    return -1;
  }

  Shell_NotifyIcon(NIM_DELETE, &notify_icon_);
  if (!Shell_NotifyIcon(NIM_ADD, &notify_icon_)) {
    LOG(ERROR) << "Shell_NotifyIcon failed";
    message.LoadString(IDS_ERR_INIT_FAILED);
    MessageBox(message, nullptr, MB_ICONERROR);
    return -1;
  }

  if (RunTimeHelper::IsWin7() &&
      !Shell_NotifyIcon(NIM_SETVERSION, &notify_icon_)) {
    old_windows_ = true;
    LOG(WARNING) << "NIM_SETVERSION failed.";
  }

  if (!TunnelingService::Init()) {
    ATLASSERT(false);
    LOG(ERROR) << "TunnelingService::Init failed";
    message.LoadString(IDS_ERR_INIT_FAILED);
    MessageBox(message, nullptr, MB_ICONERROR);
    return -1;
  }

  service_manager_.reset(new ServiceManager());
  ATLASSERT(service_manager_ != nullptr);
  if (service_manager_ == nullptr) {
    LOG(ERROR) << "ServiceManager cannot be allocated";
    message.LoadString(IDS_ERR_OUT_OF_MEMORY);
    MessageBox(message, nullptr, MB_ICONERROR);
    return -1;
  }

  if (!service_manager_->LoadServices()) {
    message.LoadString(IDS_ERR_INIT_FAILED);
    MessageBox(message, nullptr, MB_ICONERROR);
    return -1;
  }

  auto some_failed = false;

  if (!service_manager_->LoadServers())
    some_failed = true;

  if (!service_manager_->StartServers())
    some_failed = true;

  if (some_failed) {
    message.LoadString(IDS_ERR_START_FAILED);
    MessageBox(message, nullptr, MB_ICONEXCLAMATION);
  }

  return 0;
}

void MainFrame::OnDestroy() {
  SetMsgHandled(FALSE);

  LoadIconMetric(ModuleHelper::GetResourceInstance(),
                 MAKEINTRESOURCE(IDR_TRAY_MENU), LIM_SMALL,
                 &notify_icon_.hIcon);
  Shell_NotifyIcon(NIM_MODIFY, &notify_icon_);

  if (service_manager_)
    service_manager_->StopServers();

  Shell_NotifyIcon(NIM_DELETE, &notify_icon_);

  if (service_manager_)
    service_manager_->StopServices();

  TunnelingService::Term();
}

void MainFrame::OnEndSession(BOOL ending, UINT /*log_off*/) {
  if (ending)
    DestroyWindow();
}

LRESULT MainFrame::OnTrayNotify(UINT message, WPARAM wParam, LPARAM lParam) {
  if (old_windows_)
    return OnOldTrayNotify(message, wParam, lParam);

  switch (LOWORD(lParam)) {
    case WM_LBUTTONDBLCLK:
      SetForegroundWindow(m_hWnd);
      PostMessage(WM_COMMAND, MAKEWPARAM(kDefaultTrayCommand, 0));
      break;

    case WM_CONTEXTMENU:
      TrackTrayMenu(GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
      break;
  }

  return 0;
}

LRESULT MainFrame::OnOldTrayNotify(UINT /*message*/, WPARAM /*wParam*/,
                                   LPARAM lParam) {
  switch (lParam) {
    case WM_LBUTTONDBLCLK:
      SetForegroundWindow(m_hWnd);
      PostMessage(WM_COMMAND, MAKEWPARAM(kDefaultTrayCommand, 0));
      break;

    case WM_RBUTTONUP: {
      POINT point;
      GetCursorPos(&point);
      TrackTrayMenu(point.x, point.y);
      break;
    }
  }

  return 0;
}

LRESULT MainFrame::OnTaskbarCreated(UINT /*message*/, WPARAM /*wParam*/,
                                    LPARAM /*lParam*/) {
  Shell_NotifyIcon(NIM_ADD, &notify_icon_);
  old_windows_ = Shell_NotifyIcon(NIM_SETVERSION, &notify_icon_) == FALSE;

  return 0;
}

void MainFrame::OnFileNew(UINT /*notify_code*/, int /*id*/,
                          CWindow /*control*/) {
  if (configuring_)
    return;

  PreferenceDialog dialog;

  configuring_ = true;
  auto result = dialog.DoModal(NULL);
  configuring_ = false;

  if (result != IDOK)
    return;

  auto succeeded = service_manager_->UpdateConfiguration(
      std::move(dialog.service_configs_), std::move(dialog.server_configs_));
  if (!succeeded) {
    CString message;
    message.LoadString(IDS_ERR_START_FAILED);
    MessageBox(message, nullptr, MB_ICONERROR);
  }
}

void MainFrame::OnAppExit(UINT /*notify_code*/, int /*id*/,
                          CWindow /*control*/) {
  PostMessage(WM_CLOSE);
}
