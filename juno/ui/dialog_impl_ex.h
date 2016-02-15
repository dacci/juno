// Copyright (c) 2016 dacci.org

#ifndef JUNO_UI_DIALOG_IMPL_EX_H_
#define JUNO_UI_DIALOG_IMPL_EX_H_

#include <atlbase.h>
#include <atlwin.h>

#include <atlapp.h>
#include <atlctrls.h>
#include <atlmisc.h>

template<class T, class TBase = CWindow>
class ATL_NO_VTABLE DialogImplEx : public CDialogImpl<T, TBase> {
  BEGIN_MSG_MAP(DialogImplEx)
    MESSAGE_HANDLER(WM_TIMER, OnTimer)
  END_MSG_MAP()

 protected:
  HRESULT TaskDialog(_U_STRINGorID window_title,
                     _U_STRINGorID main_instruction,
                     _U_STRINGorID content,
                     TASKDIALOG_COMMON_BUTTON_FLAGS buttons,
                     _U_STRINGorID icon,
                     int* answer) {
    return ::TaskDialog(m_hWnd, ModuleHelper::GetResourceInstance(),
                        window_title.m_lpstr, main_instruction.m_lpstr,
                        content.m_lpstr, buttons, icon.m_lpstr, answer);
  }

  void ShowBalloonTip(const CWindow& control, UINT text) {
    if (!control.IsWindow())
      return;

    if (!balloon_tool_tip_.IsWindow()) {
      balloon_tool_tip_.Create(m_hWnd, nullptr, nullptr, TTS_BALLOON);
      if (!balloon_tool_tip_.IsWindow())
        return;
    }

    CToolInfo tool_info(
        TTF_SUBCLASS | TTF_TRACK | TTF_ABSOLUTE | TTF_TRANSPARENT, control);
    tool_info.hinst = ModuleHelper::GetResourceInstance();
    tool_info.lpszText = MAKEINTRESOURCE(text);
    if (!balloon_tool_tip_.AddTool(&tool_info))
      return;

    balloon_tool_tip_.SetMaxTipWidth(400);
    balloon_tool_tip_.SetTitle(0U, nullptr);

    CRect rect;
    if (!control.GetWindowRect(&rect))
      return;

    balloon_tool_tip_.TrackPosition(rect.left + rect.Width() / 2,
                                    rect.top + rect.Height() / 2);
    balloon_tool_tip_.TrackActivate(&tool_info, TRUE);

    SetTimer(kBalloonTimerId, 10 * 1000);
  }

  void HideBalloonTip() {
    if (balloon_tool_tip_.IsWindow()) {
      KillTimer(kBalloonTimerId);
      balloon_tool_tip_.TrackActivate(nullptr, FALSE);
    }
  }

 private:
  const int kBalloonTimerId = rand();  // NOLINT(runtime/threadsafe_fn)

  LRESULT OnTimer(UINT /*message*/, WPARAM wParam, LPARAM /*lParam*/,
                  BOOL& handled) {  // NOLINT(runtime/references)
    if (wParam == kBalloonTimerId)
      HideBalloonTip();
    else
      handled = FALSE;

    return 0;
  }

  CToolTipCtrl balloon_tool_tip_;
};

#endif  // JUNO_UI_DIALOG_IMPL_EX_H_
