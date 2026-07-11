#pragma once
#include "gguf_reader.h"
#include "gui.h"
#include "wx/dataview.h"
#include "wx/string.h"

struct GApp;
struct GFrame : wxFrame {
  wxIcon icon;
  wxTaskBarIcon taskbaricon;

  GGUFReader reader;

  // UI Controls
  wxNotebook *notebook;
  wxDataViewListCtrl *metadataTable;
  wxDataViewListCtrl *tensorTable;

  GFrame();
  // void MyAppend(wxMenu *menu, int tag, const wxString &contents,
  //               const wxString &help = "");

  

  void OnExit(wxCommandEvent &event);
  void OnDrop(wxDropFilesEvent &event);
  void OnMenu(wxCommandEvent &event);
  void OnMetadataItemActivated(wxDataViewEvent &event);
  void OnMetadataSelectionChanged(wxDataViewEvent &event);
  void updateTables(const wxString &filepath);

  wxDECLARE_EVENT_TABLE();
};
