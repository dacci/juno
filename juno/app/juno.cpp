// Copyright (c) 2013 dacci.org

#include "app/juno.h"

#include <crtdbg.h>
#include <stdint.h>

#include <madoka/net/winsock.h>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/logging_win.h>

#include <url/url_util.h>

#include "app/service_manager.h"
#include "net/tunneling_service.h"
#include "ui/main_frame.h"

// {303373E4-6763-4780-B199-5325DFAFEFDD}
const GUID GUID_JUNO_APPLICATION = {
  0x303373e4, 0x6763, 0x4780, { 0xb1, 0x99, 0x53, 0x25, 0xdf, 0xaf, 0xef, 0xdd }
};

CAppModule _Module;

int __stdcall wWinMain(HINSTANCE hInstance, HINSTANCE, wchar_t*, int) {
#ifdef _DEBUG
  {
    int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    flags |= _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF;
    _CrtSetDbgFlag(flags);
  }
#endif  // _DEBUG

  ::SetDllDirectory(_T(""));
  ::SetSearchPathMode(BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE |
                      BASE_SEARCH_PATH_PERMANENT);

  ::HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, nullptr, 0);

  CString guid;
  int length = ::StringFromGUID2(GUID_JUNO_APPLICATION, guid.GetBuffer(40), 40);
  guid.ReleaseBuffer(length);

  HANDLE mutex = ::CreateMutex(nullptr, TRUE, guid);
  DWORD error = ::GetLastError();
  if (mutex == NULL)
    return __LINE__;
  if (error != ERROR_SUCCESS) {
    ::CloseHandle(mutex);
    return __LINE__;
  }

  HRESULT result = S_OK;

  result = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  ATLASSERT(SUCCEEDED(result));
  if (FAILED(result))
    return __LINE__;

  result = ::AtlInitCommonControls(0xFFFF);  // all classes
  ATLASSERT(result != FALSE);
  if (!result)
    return __LINE__;

  madoka::net::WinSock winsock(WINSOCK_VERSION);
  ATLASSERT(winsock.Initialized());
  if (!winsock.Initialized())
    return __LINE__;

  base::AtExitManager atexit_manager;
  base::CommandLine::Init(0, nullptr);

  logging::LoggingSettings logging_settings;
  logging_settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(logging_settings);

  logging::LogEventProvider::Initialize(GUID_JUNO_APPLICATION);

  url::Initialize();

  if (!TunnelingService::Init()) {
    ATLASSERT(false);
    return __LINE__;
  }

  service_manager = new ServiceManager();
  ATLASSERT(service_manager != nullptr);
  if (service_manager == nullptr)
    return __LINE__;

  result = _Module.Init(nullptr, hInstance);
  ATLASSERT(SUCCEEDED(result));
  if (FAILED(result))
    return __LINE__;

  {
    CMessageLoop message_loop;
    _Module.AddMessageLoop(&message_loop);

    MainFrame frame;
    if (frame.CreateEx())
      message_loop.Run();

    _Module.RemoveMessageLoop();
  }

  ::CloseHandle(mutex);
  _Module.Term();
  delete service_manager;
  TunnelingService::Term();
  url::Shutdown();
  logging::LogEventProvider::Uninitialize();
  base::CommandLine::Reset();
  ::CoUninitialize();

  return 0;
}
