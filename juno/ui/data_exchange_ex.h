// Copyright (c) 2017 dacci.org

#ifndef JUNO_UI_DATA_EXCHANGE_EX_H_
#define JUNO_UI_DATA_EXCHANGE_EX_H_

#include <atlbase.h>
#include <atlwin.h>

#include <atlapp.h>
#include <atlddx.h>

#include <string>

namespace juno {
namespace ui {

template <class T>
class DataExchangeEx : public CWinDataExchange<T> {
 public:
  BOOL DDX_Text(UINT id, std::wstring& text,  // NOLINT(runtime/references)
                int /*size*/, BOOL save, BOOL validate = FALSE,
                int length = 0) {
    auto self = static_cast<T*>(this);
    auto success = TRUE;

    if (save) {
      auto control = self->GetDlgItem(id);
      auto text_length = ::GetWindowTextLengthW(control);
      auto returned_length = -1;
      text.resize(text_length);
      returned_length = ::GetWindowTextW(control, &text[0], text_length + 1);
      if (returned_length < text_length)
        success = FALSE;
    } else {
      success = ::SetDlgItemTextW(self->m_hWnd, id, text.c_str());
    }

    if (!success) {
      self->OnDataExchangeError(id, save);
    } else if (save && validate) {
      ATLASSERT(length > 0);
      if (static_cast<int>(text.size()) > length) {
        _XData data{ddxDataText};
        data.textData.nLength = static_cast<int>(text.size());
        data.textData.nMaxLength = length;
        self->OnDataValidateError(id, save, data);
        success = FALSE;
      }
    }

    return success;
  }
};

}  // namespace ui
}  // namespace juno

#endif  // JUNO_UI_DATA_EXCHANGE_EX_H_
