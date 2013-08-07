// Copyright (c) 2013 dacci.org

#include "ui/main_frame.h"

#include "app/juno.h"
#include "app/service_manager.h"
#include "ui/servers_page.h"
#include "ui/services_page.h"

const UINT MainFrame::WM_TASKBARCREATED =
    ::RegisterWindowMessage("TaskbarCreated");

MainFrame::MainFrame() : notify_icon_(), configuring_() {
  OSVERSIONINFOEX version_info = { sizeof(version_info) };
  version_info.dwMajorVersion = 6;
  version_info.dwMinorVersion = 1;

  DWORDLONG condition_mask = 0;
  VER_SET_CONDITION(condition_mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
  VER_SET_CONDITION(condition_mask, VER_MINORVERSION, VER_GREATER_EQUAL);

  old_windows_ = ::VerifyVersionInfo(&version_info,
                                     VER_MAJORVERSION | VER_MINORVERSION,
                                     condition_mask) == FALSE;
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
  service_manager->UnloadServers();
  service_manager->UnloadServices();
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
  ::sprintf_s(notify_icon_.szTip, "Juno");
  notify_icon_.uVersion = NOTIFYICON_VERSION_4;
  notify_icon_.guidItem = GUID_JUNO_APPLICATION;
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

  case WM_RBUTTONUP:
    {
      POINT point;
      ::GetCursorPos(&point);
      TrackTrayMenu(point.x, point.y);
    }
    break;
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

  CString caption;
  caption.LoadString(ID_FILE_NEW);

  CPropertySheet property_sheet(caption.GetString());
  property_sheet.AddPage(*new ServicesPage());
  property_sheet.AddPage(*new ServersPage());

  configuring_ = true;
  int result = property_sheet.DoModal(m_hWnd);
  configuring_ = false;

  if (result != IDOK)
    return;

  caption.LoadString(IDS_CONFIRM_APPLY);
  if (MessageBox(caption, NULL, MB_ICONQUESTION | MB_YESNO) != IDYES)
    return;

  StopAndUnload();
  LoadAndStart();
}

void MainFrame::OnAppExit(UINT notify_code, int id, CWindow control) {
  PostMessage(WM_CLOSE);
}
