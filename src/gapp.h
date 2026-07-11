#pragma once
#include "gui.h"


struct GApp : public wxApp {
  virtual bool OnInit() override;

#ifdef __WXMSW__
  void DeclareHiDpiAwareOnWindows();
#endif
};
