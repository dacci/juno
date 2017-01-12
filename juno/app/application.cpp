// Copyright (c) 2016 dacci.org

#include "app/application.h"

#include <atlstr.h>

#include <base/command_line.h>
#include <base/logging.h>

#pragma warning(push, 3)
#pragma warning(disable : 4244)
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/win/scoped_handle.h>
#pragma warning(pop)

#include <url/url_util.h>

#include "app/constants.h"
#include "misc/tunneling_service.h"
#include "service/rpc/rpc_service.h"
#include "service/service_manager.h"
#include "ui/main_frame.h"

namespace juno {
namespace app {
namespace {

class ServiceHandleTraits {
 public:
  typedef SC_HANDLE Handle;

  static bool CloseHandle(Handle handle) {
    return CloseServiceHandle(handle) != FALSE;
  }

  static bool IsHandleValid(Handle handle) {
    return handle != NULL;
  }

  static Handle NullHandle() {
    return NULL;
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ServiceHandleTraits);
};

typedef base::win::GenericScopedHandle<ServiceHandleTraits,
                                       base::win::VerifierTraits>
    ScopedServiceHandle;

}  // namespace

Application::Application()
    : service_mode_(false),
      foreground_mode_(false),
      main_thread_id_(0),
      status_handle_(NULL),
      service_status_(),
      event_source_(NULL),
      mutex_(NULL),
      message_loop_(nullptr),
      service_manager_(nullptr),
      frame_(nullptr) {
  service_status_.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  service_status_.dwControlsAccepted = SERVICE_ACCEPT_STOP;
}

Application::~Application() {
  base::CommandLine::Reset();
}

HRESULT Application::InstallService() {
  HRESULT result;

  do {
    ScopedServiceHandle manager(
        OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
    if (!manager.IsValid()) {
      result = HRESULT_FROM_WIN32(GetLastError());
      LOG(ERROR) << "Failed to open service control manager: 0x" << std::hex
                 << result;
      break;
    }

    ScopedServiceHandle service(
        OpenService(manager.Get(), kServiceName, SERVICE_ALL_ACCESS));
    if (service.IsValid()) {
      result = S_FALSE;
      LOG(INFO) << "Service already installed.";
      break;
    }

    auto command_line = base::CommandLine::ForCurrentProcess();
    auto path = command_line->GetProgram();
    if (!path.IsAbsolute())
      path = base::MakeAbsoluteFilePath(path);

    base::CommandLine service_command(path);
    service_command.AppendSwitch(switches::kService);

    service.Set(CreateServiceW(manager.Get(), kServiceName, kServiceName,
                               SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                               SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE,
                               service_command.GetCommandLineString().c_str(),
                               nullptr, nullptr, L"tcpip\0", nullptr, nullptr));
    if (!service.IsValid()) {
      result = HRESULT_FROM_WIN32(GetLastError());
      LOG(ERROR) << "Failed to create service: 0x" << std::hex << result;
      break;
    }

    CString key_path;
    key_path.Format(
        L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\%s",
        kServiceName);

    auto value = path.value();
    auto length = (value.size() + 1) * sizeof(wchar_t);

    auto status = SHSetValue(HKEY_LOCAL_MACHINE, key_path, L"EventMessageFile",
                             REG_SZ, value.c_str(), static_cast<DWORD>(length));
    if (status != ERROR_SUCCESS) {
      result = HRESULT_FROM_WIN32(status);
      LOG(ERROR) << "Failed to install event source: " << status;
      break;
    }

    result = S_OK;
  } while (false);

  return result;
}

HRESULT Application::UninstallService() {
  HRESULT result;

  do {
    ScopedServiceHandle manager(
        OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
    if (!manager.IsValid()) {
      result = HRESULT_FROM_WIN32(GetLastError());
      LOG(ERROR) << "Failed to open service control manager: 0x" << std::hex
                 << result;
      break;
    }

    ScopedServiceHandle service(
        OpenService(manager.Get(), kServiceName, SERVICE_ALL_ACCESS));
    if (!service.IsValid()) {
      result = HRESULT_FROM_WIN32(GetLastError());
      LOG(INFO) << "Failed to open service: 0x" << std::hex << result;
      break;
    }

    if (!DeleteService(service.Get())) {
      result = HRESULT_FROM_WIN32(GetLastError());
      LOG(ERROR) << "Failed to delete service: 0x" << std::hex << result;
      break;
    }

    CString key_path;
    key_path.Format(
        L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\%s",
        kServiceName);

    auto status = SHDeleteKey(HKEY_LOCAL_MACHINE, key_path);
    if (status != ERROR_SUCCESS) {
      result = HRESULT_FROM_WIN32(status);
      LOG(ERROR) << "Failed to remove event source: " << status;
      break;
    }

    result = S_OK;
  } while (false);

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
    ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_INIT_FAILED);
    return false;
  }

  logging::LoggingSettings logging_settings;
  logging_settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  succeeded = logging::InitLogging(logging_settings);
  ATLASSERT(succeeded);
  if (!succeeded) {
    *result = E_FAIL;
    OutputDebugString(L"Failed to initialize logging.\n");
    ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_INIT_FAILED);
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

  message_loop_ = new CMessageLoop();
  if (message_loop_ == nullptr) {
    LOG(ERROR) << "Failed to allocate message loop.";
    ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_INIT_FAILED);
    return S_FALSE;
  }

  if (service_mode_) {
    event_source_ = RegisterEventSource(nullptr, kServiceName);
    if (event_source_ == NULL) {
      LOG(ERROR) << "Failed to register event source: " << GetLastError();
      return S_FALSE;
    }

    rpc_service_ = new service::rpc::RpcService();
    if (rpc_service_ == nullptr) {
      LOG(ERROR) << "Failed to allocate RpcService.";
      ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_INIT_FAILED);
      return S_FALSE;
    }

    result = rpc_service_->Start(kRpcServiceName);
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to start RPC service: 0x" << std::hex << result;
      ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_INIT_FAILED);
      return S_FALSE;
    }
  }

  if (!service_mode_ && !foreground_mode_) {
    wchar_t mutex_name[40];
    auto count = StringFromGUID2(GUID_JUNO_APPLICATION, mutex_name,
                                 _countof(mutex_name));
    if (count == 0) {
      LOG(ERROR) << "StringFromGUID2() failed.";
      ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_INIT_FAILED);
      return S_FALSE;
    }

    mutex_ = CreateMutex(nullptr, FALSE, mutex_name);
    if (mutex_ == NULL) {
      LOG(ERROR) << "Failed to create mutex: " << GetLastError();
      ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_INIT_FAILED);
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

      ReportEvent(EVENTLOG_ERROR_TYPE, message_id);
      return S_FALSE;
    }
  }

  WSADATA wsa_data;
  auto error = WSAStartup(WINSOCK_VERSION, &wsa_data);
  if (error == 0) {
    DLOG(INFO) << wsa_data.szDescription << " " << wsa_data.szSystemStatus;
  } else {
    LOG(ERROR) << "WinSock startup failed: " << error;
    ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_INIT_FAILED);
    return S_FALSE;
  }

  url::Initialize();

  result = misc::TunnelingService::Init();
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to initialize TunnelingService: 0x" << std::hex
               << result;
    ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_INIT_FAILED);
    return S_FALSE;
  }

  service_manager_ = new service::ServiceManager();
  if (service_manager_ == nullptr) {
    LOG(ERROR) << "Failed to allocate ServiceManager.";
    ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_OUT_OF_MEMORY);
    return S_FALSE;
  }

  if (!service_manager_->LoadServices()) {
    LOG(ERROR) << "Failed to load services.";
    ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_INIT_FAILED);
    return S_FALSE;
  }

  auto some_failed = false;

  if (!service_manager_->LoadServers()) {
    LOG(WARNING) << "Failed to load some of the servers.";
    some_failed = true;
  }

  if (!service_manager_->StartServers()) {
    LOG(WARNING) << "Failed to start some of the servers.";
    some_failed = true;
  }

  if (some_failed)
    ReportEvent(EVENTLOG_WARNING_TYPE, IDS_ERR_START_FAILED);

  if (!service_mode_) {
    frame_ = new ui::MainFrame();
    if (frame_ == nullptr) {
      LOG(ERROR) << "Failed to allocate main frame.";
      ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_INIT_FAILED);
      return S_FALSE;
    }

    if (frame_->CreateEx() == NULL) {
      LOG(ERROR) << "Failed to create main frame.";
      ReportEvent(EVENTLOG_ERROR_TYPE, IDS_ERR_INIT_FAILED);
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

  if (service_manager_ != nullptr) {
    service_manager_->StopServers();
    service_manager_->StopServices();

    delete service_manager_;
    service_manager_ = nullptr;
  }

  misc::TunnelingService::Term();
  url::Shutdown();
  WSACleanup();

  if (mutex_ != NULL) {
    ReleaseMutex(mutex_);
    CloseHandle(mutex_);
    mutex_ = NULL;
  }

  if (rpc_service_ != nullptr) {
    rpc_service_->Stop();
    delete rpc_service_;
    rpc_service_ = nullptr;
  }

  if (event_source_ != NULL) {
    DeregisterEventSource(event_source_);
    event_source_ = NULL;
  }

  if (message_loop_ != nullptr) {
    delete message_loop_;
    message_loop_ = nullptr;
  }

  return CAtlExeModuleT::PostMessageLoop();
}

void Application::RunMessageLoop() throw() {
  CHECK(message_loop_ != nullptr) << "Something wrong.";
  main_thread_id_ = GetCurrentThreadId();
  message_loop_->Run();
}

void Application::ReportEvent(WORD type, DWORD message_id) const {
  CString message;
  message.LoadString(message_id);

  if (service_mode_) {
    if (event_source_ != NULL) {
      DWORD event_id;
      switch (type) {
        case EVENTLOG_ERROR_TYPE:
        case EVENTLOG_AUDIT_FAILURE:
          event_id = 3;
          break;

        case EVENTLOG_WARNING_TYPE:
          event_id = 2;
          break;

        case EVENTLOG_INFORMATION_TYPE:
        default:
          event_id = 1;
          break;

        case EVENTLOG_SUCCESS:
        case EVENTLOG_AUDIT_SUCCESS:
          event_id = 0;
          break;
      }

      auto pointer = message.GetString();
      ::ReportEvent(event_source_, type, 0, event_id, nullptr, 1, 0, &pointer,
                    nullptr);
    }
  } else {
    PCTSTR icon;
    switch (type) {
      case EVENTLOG_ERROR_TYPE:
      case EVENTLOG_AUDIT_FAILURE:
        icon = TD_ERROR_ICON;
        break;

      case EVENTLOG_WARNING_TYPE:
        icon = TD_WARNING_ICON;
        break;

      case EVENTLOG_SUCCESS:
      case EVENTLOG_INFORMATION_TYPE:
      case EVENTLOG_AUDIT_SUCCESS:
        icon = TD_INFORMATION_ICON;
        break;

      default:
        icon = nullptr;
        break;
    }

    HWND parent = NULL;
    if (foreground_mode_ && frame_ != nullptr)
      parent = frame_->m_hWnd;

    TaskDialog(parent, ModuleHelper::GetResourceInstance(),
               MAKEINTRESOURCE(IDR_MAIN_FRAME), nullptr,
               MAKEINTRESOURCE(message_id), TDCBF_OK_BUTTON, icon, nullptr);
  }
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
  GetApplication()->ServiceMain();
}

void Application::ServiceMain() {
  status_handle_ =
      RegisterServiceCtrlHandlerEx(kServiceName, ServiceHandler, nullptr);
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
                                  void* /*context*/) {
  return GetApplication()->ServiceHandler(control, type, data);
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
