// Copyright (c) 2016 dacci.org

#include <initguid.h>

#include "app/constants.h"

const wchar_t kServiceName[] = L"Juno";
const wchar_t kRpcServiceName[] = L"juno.rpc";

const char kConfigGetMethod[] = "config.get";
const char kConfigSetMethod[] = "config.set";

namespace switches {

const char kForeground[] = "foreground";

const char kService[] = "service";
const char kInstall[] = "install";
const char kUninstall[] = "uninstall";

}  // namespace switches
