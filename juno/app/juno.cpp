// Copyright (c) 2013 dacci.org

#include "app/juno.h"

#include <crtdbg.h>
#include <stdint.h>

#include <atlstr.h>

#include <initguid.h>

#include <madoka/net/winsock.h>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/logging_win.h>

#include <url/url_util.h>

#include "app/constants.h"
#include "ui/main_frame.h"

CAppModule _Module;

int __stdcall wWinMain(HINSTANCE hInstance, HINSTANCE, wchar_t*, int) {
#ifdef _DEBUG
  {
    int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    flags |= _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF;
    _CrtSetDbgFlag(flags);
  }
#endif  // _DEBUG

  SetDllDirectory(_T(""));
  SetSearchPathMode(BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE |
                    BASE_SEARCH_PATH_PERMANENT);

  HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, nullptr, 0);

  CString message;
  message.LoadString(IDS_ERR_INIT_FAILED);

  HRESULT result = S_OK;

  result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  ATLASSERT(SUCCEEDED(result));
  if (FAILED(result)) {
    LOG(ERROR) << "CoInitializeEx failed: 0x" << std::hex << result;
    MessageBox(NULL, message, nullptr, MB_ICONERROR);
    return __LINE__;
  }

  result = AtlInitCommonControls(0xFFFF);  // all classes
  ATLASSERT(result != FALSE);
  if (!result) {
    LOG(ERROR) << "AtlInitCommonControls failed";
    MessageBox(NULL, message, nullptr, MB_ICONERROR);
    return __LINE__;
  }

  madoka::net::WinSock winsock(WINSOCK_VERSION);
  ATLASSERT(winsock.Initialized());
  if (!winsock.Initialized()) {
    LOG(ERROR) << "WinSock startup failed: " << winsock.error();
    MessageBox(NULL, message, nullptr, MB_ICONERROR);
    return __LINE__;
  }

  base::AtExitManager atexit_manager;
  base::CommandLine::Init(0, nullptr);

  logging::LoggingSettings logging_settings;
  logging_settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(logging_settings);

  logging::LogEventProvider::Initialize(GUID_JUNO_APPLICATION);

  url::Initialize();

  result = _Module.Init(nullptr, hInstance);
  ATLASSERT(SUCCEEDED(result));
  if (FAILED(result)) {
    LOG(ERROR) << "CAppModule::Init failed: 0x" << std::hex << result;
    MessageBox(NULL, message, nullptr, MB_ICONERROR);
    return __LINE__;
  }

  {
    CMessageLoop message_loop;
    _Module.AddMessageLoop(&message_loop);

    MainFrame frame;
    if (frame.CreateEx())
      message_loop.Run();

    _Module.RemoveMessageLoop();
  }

  _Module.Term();
  url::Shutdown();
  logging::LogEventProvider::Uninitialize();
  base::CommandLine::Reset();
  CoUninitialize();

  return 0;
}
