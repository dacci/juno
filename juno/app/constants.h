// Copyright (c) 2015 dacci.org

#ifndef JUNO_APP_CONSTANTS_H_
#define JUNO_APP_CONSTANTS_H_

#include <guiddef.h>

// {303373E4-6763-4780-B199-5325DFAFEFDD}
DEFINE_GUID(GUID_JUNO_APPLICATION, 0x303373e4, 0x6763, 0x4780, 0xb1, 0x99, 0x53,
            0x25, 0xdf, 0xaf, 0xef, 0xdd);

extern const wchar_t kServiceName[];
extern const wchar_t kRpcServiceName[];

extern const char kConfigGetMethod[];
extern const char kConfigSetMethod[];

namespace switches {

extern const char kForeground[];

extern const char kService[];
extern const char kInstall[];
extern const char kUninstall[];

extern const char kConfig[];

}  // namespace switches

#endif  // JUNO_APP_CONSTANTS_H_
