// Copyright (c) 2013 dacci.org

#include "ui/main_frame.h"

#include <atlstr.h>

#include <base/command_line.h>
#include <base/logging.h>

#include "app/application.h"
#include "app/constants.h"
#include "misc/tunneling_service.h"
#include "service/service_manager.h"
#include "ui/preference_dialog.h"

namespace juno {
namespace ui {

const UINT MainFrame::WM_TASKBARCREATED =
    RegisterWindowMessage(_T("TaskbarCreated"));

MainFrame::MainFrame()
    : old_windows_(false), notify_icon_(), configuring_(false) {
  if (!AtlInitCommonControls(0xFFFF))  // all classes
    LOG(ERROR) << "Failed to initialize common controls.";
}

MainFrame::~MainFrame() {}

void MainFrame::TrackTrayMenu(int x, int y) {
  if (configuring_)
    return;

  CMenuHandle menu(GetMenu());
  auto popup_menu = menu.GetSubMenu(0);
  popup_menu.SetMenuDefaultItem(kDefaultTrayCommand);

  SetForegroundWindow(m_hWnd);
  popup_menu.TrackPopupMenu(TPM_RIGHTBUTTON, x, y, m_hWnd);
  PostMessage(WM_NULL);
}

int MainFrame::OnCreate(CREATESTRUCT* /*create_struct*/) {
  auto application = app::GetApplication();

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForeground)) {
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
      application->ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_INIT_FAILED);
      return -1;
    }

    auto length = AtlLoadString(IDR_MAIN_FRAME, notify_icon_.szTip,
                                _countof(notify_icon_.szTip));
    if (length == 0) {
      LOG(ERROR) << "LoadString() failed: " << GetLastError();
      application->ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_INIT_FAILED);
      return -1;
    }

    Shell_NotifyIcon(NIM_DELETE, &notify_icon_);
    if (!Shell_NotifyIcon(NIM_ADD, &notify_icon_)) {
      LOG(ERROR) << "Shell_NotifyIcon failed";
      application->ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_INIT_FAILED);
      return -1;
    }

    if (RunTimeHelper::IsWin7() &&
        !Shell_NotifyIcon(NIM_SETVERSION, &notify_icon_)) {
      old_windows_ = true;
      LOG(WARNING) << "NIM_SETVERSION failed.";
    }
  }

  if (!misc::TunnelingService::Init()) {
    ATLASSERT(false);
    LOG(ERROR) << "TunnelingService::Init failed";
    application->ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_INIT_FAILED);
    return -1;
  }

  service_manager_.reset(new service::ServiceManager());
  ATLASSERT(service_manager_ != nullptr);
  if (service_manager_ == nullptr) {
    LOG(ERROR) << "ServiceManager cannot be allocated";
    application->ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_OUT_OF_MEMORY);
    return -1;
  }

  if (!service_manager_->LoadServices()) {
    application->ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_INIT_FAILED);
    return -1;
  }

  auto some_failed = false;

  if (!service_manager_->LoadServers())
    some_failed = true;

  if (!service_manager_->StartServers())
    some_failed = true;

  if (some_failed) {
    application->ReportEvent(EVENTLOG_WARNING_TYPE, IDS_ERR_START_FAILED);
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

  misc::TunnelingService::Term();
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

  auto foreground =
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kForeground);
  PreferenceDialog dialog;

  configuring_ = true;
  auto result = dialog.DoModal(foreground ? m_hWnd : NULL);
  configuring_ = false;

  if (result != IDOK)
    return;

  auto succeeded = service_manager_->UpdateConfiguration(
      std::move(dialog.service_configs_), std::move(dialog.server_configs_));
  if (!succeeded) {
    app::GetApplication()->ReportEvent(EVENTLOG_WARNING_TYPE,
                                       IDS_ERR_START_FAILED);
  }
}

void MainFrame::OnAppExit(UINT /*notify_code*/, int /*id*/,
                          CWindow /*control*/) {
  PostMessage(WM_CLOSE);
}

}  // namespace ui
}  // namespace juno
