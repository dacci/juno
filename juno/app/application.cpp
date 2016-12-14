// Copyright (c) 2016 dacci.org

#include "app/application.h"

#include <base/command_line.h>
#include <base/logging.h>

#include <url/url_util.h>

#include "app/constants.h"
#include "ui/main_frame.h"

namespace juno {
namespace app {

Application::Application()
    : mutex_(NULL), message_loop_(nullptr), frame_(nullptr) {}

Application::~Application() {
  base::CommandLine::Reset();
}

bool Application::ParseCommandLine(LPCTSTR /*command_line*/,
                                   HRESULT* result) throw() {
  *result = S_OK;

  auto succeeded = base::CommandLine::Init(0, nullptr);
  ATLASSERT(succeeded);
  if (!succeeded) {
    *result = E_FAIL;
    OutputDebugString(L"Failed to initialize command line.\n");
    TaskDialog(IDR_MAIN_FRAME, nullptr, IDS_ERR_INIT_FAILED, TDCBF_OK_BUTTON,
               TD_ERROR_ICON, nullptr);
    return false;
  }

  logging::LoggingSettings logging_settings;
  logging_settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  succeeded = logging::InitLogging(logging_settings);
  ATLASSERT(succeeded);
  if (!succeeded) {
    *result = E_FAIL;
    OutputDebugString(L"Failed to initialize logging.\n");
    TaskDialog(IDR_MAIN_FRAME, nullptr, IDS_ERR_INIT_FAILED, TDCBF_OK_BUTTON,
               TD_ERROR_ICON, nullptr);
    return false;
  }

  return true;
}

HRESULT Application::PreMessageLoop(int show_mode) throw() {
  auto result = CAtlExeModuleT::PreMessageLoop(show_mode);
  if (FAILED(result)) {
    LOG(ERROR) << "CAtlExeModuleT::PreMessageLoop() returned: 0x" << std::hex
               << result;
    return result;
  }

  wchar_t mutex_name[40];
  auto count =
      StringFromGUID2(GUID_JUNO_APPLICATION, mutex_name, _countof(mutex_name));
  if (count == 0) {
    LOG(ERROR) << "StringFromGUID2() failed.";
    TaskDialog(IDR_MAIN_FRAME, nullptr, IDS_ERR_INIT_FAILED, TDCBF_OK_BUTTON,
               TD_ERROR_ICON, nullptr);
    return S_FALSE;
  }

  mutex_ = CreateMutex(nullptr, FALSE, mutex_name);
  if (mutex_ == NULL) {
    LOG(ERROR) << "Failed to create mutex: " << GetLastError();
    TaskDialog(IDR_MAIN_FRAME, nullptr, IDS_ERR_INIT_FAILED, TDCBF_OK_BUTTON,
               TD_ERROR_ICON, nullptr);
    return S_FALSE;
  }

  auto status = WaitForSingleObject(mutex_, 0);
  if (status != WAIT_OBJECT_0) {
    UINT message_id;
    if (status == WAIT_TIMEOUT) {
      message_id = IDS_ERR_ALREADY_RUNNING;
      LOG(WARNING) << "Another instance maybe running, exitting.";
    } else {
      message_id = IDS_ERR_INIT_FAILED;
      LOG(ERROR) << "Failed to get status of the mutex: " << GetLastError();
    }

    TaskDialog(IDR_MAIN_FRAME, nullptr, message_id, TDCBF_OK_BUTTON,
               TD_ERROR_ICON, nullptr);
    return S_FALSE;
  }

  if (!AtlInitCommonControls(0xFFFF)) {  // all classes
    LOG(ERROR) << "AtlInitCommonControls failed";
    TaskDialog(IDR_MAIN_FRAME, nullptr, IDS_ERR_INIT_FAILED, TDCBF_OK_BUTTON,
               TD_ERROR_ICON, nullptr);
    return S_FALSE;
  }

  WSADATA wsa_data;
  auto error = WSAStartup(WINSOCK_VERSION, &wsa_data);
  if (error == 0) {
    DLOG(INFO) << wsa_data.szDescription << " " << wsa_data.szSystemStatus;
  } else {
    LOG(ERROR) << "WinSock startup failed: " << error;
    TaskDialog(IDR_MAIN_FRAME, nullptr, IDS_ERR_INIT_FAILED, TDCBF_OK_BUTTON,
               TD_ERROR_ICON, nullptr);
    return S_FALSE;
  }

  url::Initialize();

  message_loop_ = new CMessageLoop();
  if (message_loop_ == nullptr) {
    LOG(ERROR) << "Failed to allocate message loop.";
    TaskDialog(IDR_MAIN_FRAME, nullptr, IDS_ERR_INIT_FAILED, TDCBF_OK_BUTTON,
               TD_ERROR_ICON, nullptr);
    return S_FALSE;
  }

  frame_ = new MainFrame();
  if (frame_ == nullptr) {
    LOG(ERROR) << "Failed to allocate main frame.";
    TaskDialog(IDR_MAIN_FRAME, nullptr, IDS_ERR_INIT_FAILED, TDCBF_OK_BUTTON,
               TD_ERROR_ICON, nullptr);
    return S_FALSE;
  }

  if (frame_->CreateEx() == NULL) {
    LOG(ERROR) << "Failed to create main frame.";
    TaskDialog(IDR_MAIN_FRAME, nullptr, IDS_ERR_INIT_FAILED, TDCBF_OK_BUTTON,
               TD_ERROR_ICON, nullptr);
    return S_FALSE;
  }

  return S_OK;
}

HRESULT Application::PostMessageLoop() throw() {
  if (frame_ != nullptr) {
    delete frame_;
    frame_ = nullptr;
  }

  if (message_loop_ != nullptr) {
    delete message_loop_;
    message_loop_ = nullptr;
  }

  url::Shutdown();
  WSACleanup();

  if (mutex_ != NULL) {
    ReleaseMutex(mutex_);
    CloseHandle(mutex_);
    mutex_ = NULL;
  }

  return CAtlExeModuleT::PostMessageLoop();
}

void Application::RunMessageLoop() throw() {
  CHECK(message_loop_ != nullptr) << "Something wrong.";
  message_loop_->Run();
}

}  // namespace app
}  // namespace juno

int __stdcall wWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/,
                       wchar_t* /*command_line*/, int show_mode) {
  HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, nullptr, 0);

  SetSearchPathMode(BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE |
                    BASE_SEARCH_PATH_PERMANENT);
  SetDllDirectory(_T(""));

#ifdef _DEBUG
  {
    auto flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    flags |=
        _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF;
    _CrtSetDbgFlag(flags);
  }
#endif  // _DEBUG

  return juno::app::Application().WinMain(show_mode);
}
