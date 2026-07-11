#include "gapp.h"
#include "gframe.h"

bool GApp::OnInit() {
#ifdef __WXMSW__
  DeclareHiDpiAwareOnWindows();
#endif
  SetAppName("GGUF Inspector");
  // SetAppearance(Appearance::Dark);
  wxFrame *frame = new GFrame();
  frame->SetClientSize(1200, 900);
  frame->Show(true);
  SetTopWindow(frame);
  return true;
}


#ifdef __WXMSW__
void GApp::DeclareHiDpiAwareOnWindows() {
  // dont't want to use manifest dammit

#ifndef DPI_ENUMS_DECLARED
  typedef enum PROCESS_DPI_AWARENESS {
    PROCESS_DPI_UNAWARE = 0,
    PROCESS_SYSTEM_DPI_AWARE = 1,
    PROCESS_PER_MONITOR_DPI_AWARE = 2
  } PROCESS_DPI_AWARENESS;
#endif

  using SetProcessDPIAware_T = BOOL(WINAPI *)(void);
  using SetProcessDpiAwareness_T = HRESULT(WINAPI *)(PROCESS_DPI_AWARENESS);
  using SetProcessDpiAwarenessContext_T = BOOL(WINAPI *)(DPI_AWARENESS_CONTEXT);

  SetProcessDPIAware_T SetProcessDPIAware = nullptr;
  SetProcessDpiAwareness_T SetProcessDpiAwareness = nullptr;
  SetProcessDpiAwarenessContext_T SetProcessDpiAwarenessContext = nullptr;

  HMODULE user32 = LoadLibraryA("User32.dll");
  HMODULE shcore = LoadLibraryA("Shcore.dll");

  if (user32) {
    SetProcessDPIAware =
        (SetProcessDPIAware_T)GetProcAddress(user32, "SetProcessDPIAware");
    SetProcessDpiAwarenessContext =
        (SetProcessDpiAwarenessContext_T)GetProcAddress(
            user32, "SetProcessDpiAwarenessContext");
  }
  if (shcore) {
    SetProcessDpiAwareness = (SetProcessDpiAwareness_T)GetProcAddress(
        shcore, "SetProcessDpiAwareness");
  }

  if (SetProcessDpiAwarenessContext) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  } else if (SetProcessDpiAwareness) {
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
  } else if (SetProcessDPIAware) {
    SetProcessDPIAware();
  }

  if (user32)
    FreeLibrary(user32);
  if (shcore)
    FreeLibrary(shcore);
}

#endif