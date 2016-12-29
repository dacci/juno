// Copyright (c) 2016 dacci.org

#include "app/application.h"

#include <base/command_line.h>
#include <base/logging.h>

#pragma warning(push, 3)
#pragma warning(disable : 4244)
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#pragma warning(pop)

#include <url/url_util.h>

#include "app/constants.h"
#include "ui/main_frame.h"

namespace juno {
namespace app {

Application::Application()
    : service_mode_(false),
      foreground_mode_(false),
      main_thread_id_(0),
      status_handle_(NULL),
      service_status_(),
      mutex_(NULL),
      message_loop_(nullptr),
      frame_(nullptr) {
  service_status_.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  service_status_.dwControlsAccepted = SERVICE_ACCEPT_STOP;
}

Application::~Application() {
  base::CommandLine::Reset();
}

HRESULT Application::InstallService() {
  HRESULT result;

  SC_HANDLE manager = NULL;
  SC_HANDLE service = NULL;

  do {
    manager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (manager == NULL) {
      result = HRESULT_FROM_WIN32(GetLastError());
      LOG(ERROR) << "Failed to open service control manager: 0x" << std::hex
                 << result;
      break;
    }

    service = OpenService(manager, kServiceName, SERVICE_ALL_ACCESS);
    if (service != NULL) {
      result = HRESULT_FROM_WIN32(ERROR_ALREADY_REGISTERED);
      LOG(INFO) << "Service already installed.";
      break;
    }

    auto command_line = base::CommandLine::ForCurrentProcess();
    auto path = command_line->GetProgram();
    if (!path.IsAbsolute())
      path = base::MakeAbsoluteFilePath(path);

    base::CommandLine service_command(path);
    service_command.AppendSwitch(switches::kService);

    service = CreateService(manager, kServiceName, kServiceName,
                            SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                            SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE,
                            service_command.GetCommandLineString().c_str(),
                            nullptr, nullptr, L"tcpip\0", nullptr, nullptr);
    if (service == NULL) {
      result = HRESULT_FROM_WIN32(GetLastError());
      LOG(ERROR) << "Failed to create service: 0x" << std::hex << result;
      break;
    }

    result = S_OK;
  } while (false);

  if (service != NULL) {
    CloseServiceHandle(service);
    service = NULL;
  }

  if (manager != NULL) {
    CloseServiceHandle(manager);
    service = NULL;
  }

  return result;
}

HRESULT Application::UninstallService() {
  HRESULT result;

  SC_HANDLE manager = NULL;
  SC_HANDLE service = NULL;

  do {
    manager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (manager == NULL) {
      result = HRESULT_FROM_WIN32(GetLastError());
      LOG(ERROR) << "Failed to open service control manager: 0x" << std::hex
                 << result;
      break;
    }

    service = OpenService(manager, kServiceName, SERVICE_ALL_ACCESS);
    if (service == NULL) {
      result = HRESULT_FROM_WIN32(GetLastError());
      LOG(INFO) << "Failed to open service: 0x" << std::hex << result;
      break;
    }

    if (!DeleteService(service)) {
      result = HRESULT_FROM_WIN32(GetLastError());
      LOG(ERROR) << "Failed to delete service: 0x" << std::hex << result;
      break;
    }

    result = S_OK;
  } while (false);

  if (service != NULL) {
    CloseServiceHandle(service);
    service = NULL;
  }

  if (manager != NULL) {
    CloseServiceHandle(manager);
    service = NULL;
  }

  return result;
}

int Application::WinMain(int show_mode) throw() {
  if (CAtlBaseModule::m_bInitFailed) {
    ATLASSERT(0);
    return -1;
  }

  HRESULT result;
  if (ParseCommandLine(nullptr, &result)) {
    if (service_mode_)
      result = RunService();
    else
      result = Run(show_mode);
  }

#if defined(_DEBUG) && !defined(_ATL_NO_WIN_SUPPORT)
  _AtlWinModule.Term();
#endif

  return result;
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

  auto command_line = base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(switches::kInstall)) {
    *result = InstallService();
    return false;
  }

  if (command_line->HasSwitch(switches::kUninstall)) {
    *result = UninstallService();
    return false;
  }

  service_mode_ = command_line->HasSwitch(switches::kService);
  foreground_mode_ = command_line->HasSwitch(switches::kForeground);

  return true;
}

HRESULT Application::PreMessageLoop(int show_mode) throw() {
  auto result = CAtlExeModuleT::PreMessageLoop(show_mode);
  if (FAILED(result)) {
    LOG(ERROR) << "CAtlExeModuleT::PreMessageLoop() returned: 0x" << std::hex
               << result;
    return result;
  }

  if (!service_mode_ && !foreground_mode_) {
    wchar_t mutex_name[40];
    auto count = StringFromGUID2(GUID_JUNO_APPLICATION, mutex_name,
                                 _countof(mutex_name));
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

  if (!service_mode_) {
    frame_ = new ui::MainFrame();
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

    if (foreground_mode_) {
      frame_->ShowWindow(show_mode);
      frame_->UpdateWindow();
    }
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
  main_thread_id_ = GetCurrentThreadId();
  message_loop_->Run();
}

HRESULT Application::SetServiceStatus(DWORD current_state) {
  service_status_.dwCurrentState = current_state;
  if (!::SetServiceStatus(status_handle_, &service_status_))
    return HRESULT_FROM_WIN32(GetLastError());

  return S_OK;
}

HRESULT Application::RunService() const {
  SERVICE_TABLE_ENTRY entries[]{
      {const_cast<wchar_t*>(kServiceName), ServiceMain}, {}};
  if (!StartServiceCtrlDispatcher(entries)) {
    auto result = HRESULT_FROM_WIN32(GetLastError());
    LOG(ERROR) << "Failed to start service ctrl dispatcher: 0x" << std::hex
               << result;
    return result;
  }

  return S_OK;
}

void Application::ServiceMain(DWORD /*argc*/, LPTSTR* /*argv*/) {
  static_cast<Application*>(_pAtlModule)->ServiceMain();
}

void Application::ServiceMain() {
  status_handle_ =
      RegisterServiceCtrlHandlerEx(kServiceName, ServiceHandler, this);
  if (status_handle_ == NULL) {
    LOG(ERROR) << "Failed to register service ctrl handler: 0x" << std::hex
               << GetLastError();
    return;
  }

  HRESULT result;

  result = SetServiceStatus(SERVICE_START_PENDING);
  if (FAILED(result))
    LOG(WARNING) << "Failed to set service status: 0x" << std::hex << result;

  auto service_result = PreMessageLoop(SW_HIDE);
  if (service_result == S_OK) {
    result = SetServiceStatus(SERVICE_RUNNING);
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to start service: 0x" << std::hex << result;
      return;
    }

    RunMessageLoop();
  }

  result = SetServiceStatus(SERVICE_STOP_PENDING);
  if (FAILED(result))
    LOG(WARNING) << "Failed to set service status: 0x" << std::hex << result;

  if (SUCCEEDED(service_result))
    service_result = PostMessageLoop();

  service_status_.dwWin32ExitCode = service_result;

  result = SetServiceStatus(SERVICE_STOPPED);
  if (FAILED(result))
    LOG(ERROR) << "Failed to stop service: 0x" << std::hex << result;
}

DWORD Application::ServiceHandler(DWORD control, DWORD type, void* data,
                                  void* context) {
  return static_cast<Application*>(context)->ServiceHandler(control, type,
                                                            data);
}

DWORD Application::ServiceHandler(DWORD control, DWORD /*type*/,
                                  void* /*data*/) const {
  switch (control) {
    case SERVICE_CONTROL_STOP:
      PostThreadMessage(main_thread_id_, WM_QUIT, 0, 0);
      return NO_ERROR;

    case SERVICE_CONTROL_INTERROGATE:
      return NO_ERROR;

    default:
      return ERROR_CALL_NOT_IMPLEMENTED;
  }
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
