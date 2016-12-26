// Copyright (c) 2016 dacci.org

#ifndef JUNO_APP_APPLICATION_H_
#define JUNO_APP_APPLICATION_H_

#include <atlbase.h>

#include <atlapp.h>

#include <base/at_exit.h>

namespace juno {
namespace ui {

class MainFrame;

}  // namespace ui

namespace app {

class Application : public CAtlExeModuleT<Application> {
 public:
  Application();
  ~Application();

  bool ParseCommandLine(LPCTSTR command_line, HRESULT* result) throw();
  HRESULT PreMessageLoop(int show_mode) throw();
  HRESULT PostMessageLoop() throw();
  void RunMessageLoop() throw();

  static HRESULT TaskDialog(_U_STRINGorID title, PCWSTR main_instruction,
                            _U_STRINGorID content,
                            TASKDIALOG_COMMON_BUTTON_FLAGS buttons,
                            _U_STRINGorID icon, int* button) {
    return ::TaskDialog(NULL, ModuleHelper::GetResourceInstance(),
                        title.m_lpstr, main_instruction, content.m_lpstr,
                        buttons, icon.m_lpstr, button);
  }

  CMessageLoop* GetMessageLoop() const {
    return message_loop_;
  }

 private:
  base::AtExitManager at_exit_manager_;

  HANDLE mutex_;
  CMessageLoop* message_loop_;
  ui::MainFrame* frame_;

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
};

}  // namespace app
}  // namespace juno

#endif  // JUNO_APP_APPLICATION_H_
