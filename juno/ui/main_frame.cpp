// Copyright (c) 2013 dacci.org

#include "ui/main_frame.h"

#include "app/juno.h"
#include "app/service_manager.h"
#include "ui/preference_dialog.h"

const UINT MainFrame::WM_TASKBARCREATED =
    ::RegisterWindowMessage(_T("TaskbarCreated"));

MainFrame::MainFrame()
    : old_windows_(!RunTimeHelper::IsWin7()), notify_icon_(), configuring_() {
}

MainFrame::~MainFrame() {
}

bool MainFrame::LoadAndStart() {
  if (!service_manager->LoadServices())
    return false;

  if (!service_manager->LoadServers())
    return false;

  if (!service_manager->StartServers()) {
    CString message;
    message.LoadString(IDS_ERR_START_FAILED);
    MessageBox(message, NULL, MB_ICONEXCLAMATION);
  }

  return true;
}

void MainFrame::StopAndUnload() {
  service_manager->StopServers();
  service_manager->StopServices();
}

void MainFrame::TrackTrayMenu(int x, int y) {
  if (configuring_)
    return;

  CMenu menu;
  menu.LoadMenu(IDR_TRAY_MENU);
  CMenuHandle popup_menu = menu.GetSubMenu(0);
  popup_menu.SetMenuDefaultItem(kDefaultTrayCommand);

  ::SetForegroundWindow(m_hWnd);
  popup_menu.TrackPopupMenu(TPM_RIGHTBUTTON, x, y, m_hWnd);
  PostMessage(WM_NULL);
}

int MainFrame::OnCreate(CREATESTRUCT* create_struct) {
  notify_icon_.cbSize = sizeof(notify_icon_);
  notify_icon_.hWnd = m_hWnd;
  notify_icon_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_GUID |
                        NIF_SHOWTIP;
  notify_icon_.uCallbackMessage = WM_TRAYNOTIFY;
  notify_icon_.hIcon = ::AtlLoadIconImage(IDR_MAIN_FRAME, 0, 16, 16);
  ::_stprintf_s(notify_icon_.szTip, _T("Juno"));
  notify_icon_.uVersion = NOTIFYICON_VERSION_4;
  notify_icon_.guidItem = GUID_JUNO_APPLICATION;

  ::Shell_NotifyIcon(NIM_DELETE, &notify_icon_);
  if (!::Shell_NotifyIcon(NIM_ADD, &notify_icon_) ||
      !::Shell_NotifyIcon(NIM_SETVERSION, &notify_icon_))
    return -1;

  if (!LoadAndStart())
    return -1;

  return 0;
}

void MainFrame::OnDestroy() {
  SetMsgHandled(FALSE);

  StopAndUnload();
  ::Shell_NotifyIcon(NIM_DELETE, &notify_icon_);
}

void MainFrame::OnEndSession(BOOL ending, UINT log_off) {
  if (ending)
    DestroyWindow();
}

LRESULT MainFrame::OnTrayNotify(UINT message, WPARAM wParam, LPARAM lParam) {
  if (old_windows_)
    return OnOldTrayNotify(message, wParam, lParam);

  switch (LOWORD(lParam)) {
    case WM_LBUTTONDBLCLK:
      SendMessage(WM_COMMAND, MAKEWPARAM(kDefaultTrayCommand, 0));
      break;

    case WM_CONTEXTMENU:
      TrackTrayMenu(GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
      break;
  }

  return 0;
}

LRESULT MainFrame::OnOldTrayNotify(UINT message, WPARAM wParam, LPARAM lParam) {
  switch (lParam) {
    case WM_LBUTTONDBLCLK:
      SendMessage(WM_COMMAND, MAKEWPARAM(kDefaultTrayCommand, 0));
      break;

    case WM_RBUTTONUP: {
      POINT point;
      ::GetCursorPos(&point);
      TrackTrayMenu(point.x, point.y);
      break;
    }
  }

  return 0;
}

LRESULT MainFrame::OnTaskbarCreated(UINT message, WPARAM wParam,
                                    LPARAM lParam) {
  ::Shell_NotifyIcon(NIM_ADD, &notify_icon_);
  ::Shell_NotifyIcon(NIM_SETVERSION, &notify_icon_);

  return 0;
}

void MainFrame::OnFileNew(UINT notify_code, int id, CWindow control) {
  if (configuring_)
    return;

  PreferenceDialog dialog;

  configuring_ = true;
  int result = dialog.DoModal(NULL);
  configuring_ = false;

  if (result != IDOK)
    return;

  bool succeeded = service_manager->UpdateConfiguration(
      std::move(dialog.service_configs_), std::move(dialog.server_configs_));
  if (!succeeded) {
    CString message;
    message.LoadString(IDS_ERR_START_FAILED);
    MessageBox(message, NULL, MB_ICONERROR);
  }
}

void MainFrame::OnAppExit(UINT notify_code, int id, CWindow control) {
  PostMessage(WM_CLOSE);
}
