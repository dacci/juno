// Copyright (c) 2013 dacci.org

#include "app/juno.h"

#include <crtdbg.h>
#include <stdint.h>

#include <madoka/net/winsock.h>

#include "app/service_manager.h"
#include "ui/main_frame.h"

// {303373E4-6763-4780-B199-5325DFAFEFDD}
const GUID GUID_JUNO_APPLICATION = {
  0x303373e4, 0x6763, 0x4780, { 0xb1, 0x99, 0x53, 0x25, 0xdf, 0xaf, 0xef, 0xdd }
};

CAppModule _Module;

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE, char*, int) {
#ifdef _DEBUG
  {
    int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    flags |= _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF;
    _CrtSetDbgFlag(flags);
  }
#endif  // _DEBUG

  HANDLE mutex = ::CreateMutex(NULL, TRUE, "org.dacci.juno");
  DWORD error = ::GetLastError();
  if (mutex == NULL)
    return __LINE__;
  if (error != ERROR_SUCCESS) {
    ::CloseHandle(mutex);
    return __LINE__;
  }

  HRESULT result = S_OK;

  result = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  ATLASSERT(SUCCEEDED(result));
  if (FAILED(result))
    return __LINE__;

  result = ::AtlInitCommonControls(0xFFFF);  // all classes
  ATLASSERT(result != FALSE);
  if (!result)
    return __LINE__;

  madoka::net::WinSock winsock(WINSOCK_VERSION);
  ATLASSERT(winsock.Initialized());
  if (!winsock.Initialized())
    return __LINE__;

  service_manager = new ServiceManager();
  ATLASSERT(service_manager != NULL);
  if (service_manager == NULL)
    return __LINE__;

  result = _Module.Init(NULL, hInstance);
  ATLASSERT(SUCCEEDED(result));
  if (FAILED(result))
    return __LINE__;

  {
    CMessageLoop message_loop;
    _Module.AddMessageLoop(&message_loop);

    MainFrame frame;
    if (frame.CreateEx())
      message_loop.Run();

    _Module.RemoveMessageLoop();
  }

  _Module.Term();
  delete service_manager;
  ::CoUninitialize();
  ::CloseHandle(mutex);

  return 0;
}
