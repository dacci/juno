// Copyright (c) 2013 dacci.org

#ifndef JUNO_UI_MAIN_FRAME_H_
#define JUNO_UI_MAIN_FRAME_H_

#include <atlbase.h>
#include <atlwin.h>

#include <atlapp.h>
#include <atlcrack.h>
#include <atlframe.h>

#include <memory>

#include "res/resource.h"

class ServiceManager;

class MainFrame : public CFrameWindowImpl<MainFrame> {
 public:
  MainFrame();
  ~MainFrame();  // Required as ServiceManager is incomplete.

  DECLARE_FRAME_WND_CLASS(nullptr, IDR_MAIN_FRAME)

  BEGIN_MSG_MAP(MainFrame)
    MSG_WM_CREATE(OnCreate)
    MSG_WM_DESTROY(OnDestroy)
    MSG_WM_ENDSESSION(OnEndSession)
    MESSAGE_HANDLER_EX(WM_TRAYNOTIFY, OnTrayNotify)
    MESSAGE_HANDLER_EX(WM_TASKBARCREATED, OnTaskbarCreated)

    COMMAND_ID_HANDLER_EX(ID_FILE_NEW, OnFileNew)
    COMMAND_ID_HANDLER_EX(ID_APP_EXIT, OnAppExit)

    CHAIN_MSG_MAP(CFrameWindowImpl)
  END_MSG_MAP()

 private:
  static const UINT WM_TRAYNOTIFY = WM_USER + 1;
  static const UINT WM_TASKBARCREATED;
  static const UINT kDefaultTrayCommand = ID_FILE_NEW;

  void TrackTrayMenu(int x, int y);

  int OnCreate(CREATESTRUCT* create_struct);
  void OnDestroy();
  void OnEndSession(BOOL ending, UINT log_off);
  LRESULT OnTrayNotify(UINT message, WPARAM wParam, LPARAM lParam);
  LRESULT OnOldTrayNotify(UINT message, WPARAM wParam, LPARAM lParam);
  LRESULT OnTaskbarCreated(UINT message, WPARAM wParam, LPARAM lParam);

  void OnFileNew(UINT notify_code, int id, CWindow control);
  void OnAppExit(UINT notify_code, int id, CWindow control);

  HANDLE mutex_;
  bool old_windows_;
  NOTIFYICONDATA notify_icon_;
  bool configuring_;

  std::unique_ptr<ServiceManager> service_manager_;

  MainFrame(const MainFrame&) = delete;
  MainFrame& operator=(const MainFrame&) = delete;
};

#endif  // JUNO_UI_MAIN_FRAME_H_
