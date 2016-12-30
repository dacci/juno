// Copyright (c) 2016 dacci.org

#ifndef JUNO_APP_APPLICATION_H_
#define JUNO_APP_APPLICATION_H_

#include <atlbase.h>

#include <atlapp.h>

#include <base/at_exit.h>

namespace juno {
namespace service {

class ServiceManager;

}  // namespace service

namespace ui {

class MainFrame;

}  // namespace ui

namespace app {

class Application : public CAtlExeModuleT<Application> {
 public:
  Application();
  ~Application();

  static HRESULT InstallService();
  static HRESULT UninstallService();

  int WinMain(int show_mode) throw();

  bool ParseCommandLine(LPCTSTR command_line, HRESULT* result) throw();
  HRESULT PreMessageLoop(int show_mode) throw();
  HRESULT PostMessageLoop() throw();
  void RunMessageLoop() throw();

  void ReportEvent(WORD type, DWORD message_id) const;

  bool IsService() const {
    return service_mode_;
  }

  bool IsForeground() const {
    return foreground_mode_;
  }

  CMessageLoop* GetMessageLoop() const {
    return message_loop_;
  }

  service::ServiceManager* GetServiceManager() const {
    return service_manager_;
  }

 private:
  HRESULT SetServiceStatus(DWORD current_state);
  HRESULT RunService() const;

  static void CALLBACK ServiceMain(DWORD argc, LPTSTR* argv);
  void ServiceMain();
  static DWORD CALLBACK ServiceHandler(DWORD control, DWORD type, void* data,
                                       void* context);
  DWORD ServiceHandler(DWORD control, DWORD type, void* data) const;

  base::AtExitManager at_exit_manager_;
  bool service_mode_;
  bool foreground_mode_;
  DWORD main_thread_id_;

  SERVICE_STATUS_HANDLE status_handle_;
  SERVICE_STATUS service_status_;
  HANDLE event_source_;

  HANDLE mutex_;
  CMessageLoop* message_loop_;
  service::ServiceManager* service_manager_;
  ui::MainFrame* frame_;

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
};

inline Application* GetApplication() {
  return static_cast<Application*>(_pAtlModule);
}

}  // namespace app
}  // namespace juno

#endif  // JUNO_APP_APPLICATION_H_
