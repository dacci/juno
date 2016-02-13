// Copyright (c) 2013 dacci.org

#include "app/juno.h"

#include <crtdbg.h>

#include <winsock2.h>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/logging_win.h>

#include <url/url_util.h>

#include "app/constants.h"
#include "res/resource.h"
#include "ui/main_frame.h"

CAppModule _Module;

int __stdcall wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/,
                       wchar_t* /*command_line*/, int /*show_mode*/) {
  HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, nullptr, 0);

  SetSearchPathMode(BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE |
                    BASE_SEARCH_PATH_PERMANENT);
  SetDllDirectory(_T(""));

#ifdef _DEBUG
    int debug_flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    debug_flags |= _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF;
    _CrtSetDbgFlag(debug_flags);
#endif  // _DEBUG

  base::AtExitManager at_exit_manager;

  if (base::CommandLine::Init(0, nullptr)) {
    base::AtExitManager::RegisterCallback([](void* /*param*/) {
      base::CommandLine::Reset();
    }, nullptr);
  } else {
    TaskDialog(NULL, hInstance, MAKEINTRESOURCE(IDR_MAIN_FRAME),
               nullptr, MAKEINTRESOURCE(IDS_ERR_INIT_FAILED), TDCBF_OK_BUTTON,
               TD_ERROR_ICON, nullptr);
    return __LINE__;
  }

  logging::LoggingSettings logging_settings;
  logging_settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  if (!logging::InitLogging(logging_settings)) {
    TaskDialog(NULL, hInstance, MAKEINTRESOURCE(IDR_MAIN_FRAME),
               nullptr, MAKEINTRESOURCE(IDS_ERR_INIT_FAILED), TDCBF_OK_BUTTON,
               TD_ERROR_ICON, nullptr);
    return __LINE__;
  }

  logging::LogEventProvider::Initialize(GUID_JUNO_APPLICATION);
  base::AtExitManager::RegisterCallback([](void* /*param*/) {
    logging::LogEventProvider::Uninitialize();
  }, nullptr);

  HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (SUCCEEDED(result)) {
    base::AtExitManager::RegisterCallback([](void* /*param*/) {
      CoUninitialize();
    }, nullptr);
  } else {
    LOG(ERROR) << "CoInitializeEx failed: 0x" << std::hex << result;
    TaskDialog(NULL, hInstance, MAKEINTRESOURCE(IDR_MAIN_FRAME),
               nullptr, MAKEINTRESOURCE(IDS_ERR_INIT_FAILED), TDCBF_OK_BUTTON,
               TD_ERROR_ICON, nullptr);
    return __LINE__;
  }

  if (!AtlInitCommonControls(0xFFFF)) {  // all classes
    LOG(ERROR) << "AtlInitCommonControls failed";
    TaskDialog(NULL, hInstance, MAKEINTRESOURCE(IDR_MAIN_FRAME),
               nullptr, MAKEINTRESOURCE(IDS_ERR_INIT_FAILED), TDCBF_OK_BUTTON,
               TD_ERROR_ICON, nullptr);
    return __LINE__;
  }

  WSADATA wsa_data;
  int error = WSAStartup(WINSOCK_VERSION, &wsa_data);
  if (error == 0) {
    DLOG(INFO) << wsa_data.szDescription << " is " << wsa_data.szSystemStatus;
    base::AtExitManager::RegisterCallback([](void* /*param*/) {
      WSACleanup();
    }, nullptr);
  } else {
    LOG(ERROR) << "WinSock startup failed: " << error;
    TaskDialog(NULL, hInstance, MAKEINTRESOURCE(IDR_MAIN_FRAME),
               nullptr, MAKEINTRESOURCE(IDS_ERR_INIT_FAILED), TDCBF_OK_BUTTON,
               TD_ERROR_ICON, nullptr);
    return __LINE__;
  }

  url::Initialize();
  base::AtExitManager::RegisterCallback([](void* /*param*/) {
    url::Shutdown();
  }, nullptr);

  result = _Module.Init(nullptr, hInstance);
  if (SUCCEEDED(result)) {
    base::AtExitManager::RegisterCallback([](void* /*param*/) {
      _Module.Term();
    }, nullptr);
  } else {
    LOG(ERROR) << "CAppModule::Init failed: 0x" << std::hex << result;
    TaskDialog(NULL, hInstance, MAKEINTRESOURCE(IDR_MAIN_FRAME),
               nullptr, MAKEINTRESOURCE(IDS_ERR_INIT_FAILED), TDCBF_OK_BUTTON,
               TD_ERROR_ICON, nullptr);
    return __LINE__;
  }

  CMessageLoop message_loop;
  if (!_Module.AddMessageLoop(&message_loop)) {
    LOG(ERROR) << "Failed to create message loop.";
    TaskDialog(NULL, hInstance, MAKEINTRESOURCE(IDR_MAIN_FRAME),
               nullptr, MAKEINTRESOURCE(IDS_ERR_INIT_FAILED), TDCBF_OK_BUTTON,
               TD_ERROR_ICON, nullptr);
    return __LINE__;
  }

  MainFrame frame;
  frame.CreateEx();

  message_loop.Run();

  if (!_Module.RemoveMessageLoop())
    LOG(WARNING) << "Failed to remove message loop.";

  return 0;
}
