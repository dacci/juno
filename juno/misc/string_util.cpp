// Copyright (c) 2015 dacci.org

#include "misc/string_util.h"

#include <objbase.h>

#include <base/strings/sys_string_conversions.h>

#include <string>

namespace juno {
namespace misc {

std::string GenerateGUID() {
  return base::SysWideToNativeMB(GenerateGUID16());
}

std::wstring GenerateGUID16() {
  GUID guid;
  auto result = CoCreateGuid(&guid);
  if (FAILED(result))
    return std::wstring();

  std::wstring guid_string;
  guid_string.resize(38);
  auto length = StringFromGUID2(guid, &guid_string[0],
                                static_cast<int>(guid_string.capacity()));
  if (length != 39)
    return std::wstring();

  return std::move(guid_string);
}

}  // namespace misc
}  // namespace juno
