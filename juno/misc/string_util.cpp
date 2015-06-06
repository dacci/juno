// Copyright (c) 2015 dacci.org

#include "misc/string_util.h"

#include <objbase.h>

#include <string>
#include <utility>

std::string GenerateGUID() {
  GUID guid;
  HRESULT result = CoCreateGuid(&guid);
  if (FAILED(result))
    return std::string();

  wchar_t buffer[39];
  int length = StringFromGUID2(guid, buffer, _countof(buffer));
  if (length != 39)
    return std::string();

  std::string guid_string;
  guid_string.resize(length - 1);
  length = WideCharToMultiByte(CP_ACP, 0, buffer, length - 1, &guid_string[0],
                               guid_string.size(), nullptr, nullptr);
  if (length == 0)
    return std::string();

  return std::move(guid_string);
}
