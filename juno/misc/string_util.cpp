// Copyright (c) 2015 dacci.org

#include "misc/string_util.h"

#include <objbase.h>

#include <string>
#include <utility>

namespace juno {
namespace misc {

std::string GenerateGUID() {
  GUID guid;
  auto result = CoCreateGuid(&guid);
  if (FAILED(result))
    return std::string();

  wchar_t buffer[39];
  auto length = StringFromGUID2(guid, buffer, _countof(buffer));
  if (length != 39)
    return std::string();

  std::string guid_string;
  guid_string.resize(length - 1);
  length = WideCharToMultiByte(CP_ACP, 0, buffer, length - 1, &guid_string[0],
                               static_cast<int>(guid_string.size()), nullptr,
                               nullptr);
  if (length == 0)
    return std::string();

  return std::move(guid_string);
}

}  // namespace misc
}  // namespace juno
