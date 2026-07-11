#include "gframe.h"

#include "gapp.h"
#include "gui.h"
#include "wx/filedlg.h"
#include <wx/filename.h>
#include <wx/listctrl.h>


static const int g_array_item_count = 20;
static const int g_text_len = 200;



class GGUFVirtualListViewer : public wxListView {
  const std::vector<std::string> &m_items;

public:
  GGUFVirtualListViewer(wxWindow *parent, const std::vector<std::string> &items)
      : wxListView(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                   wxLC_REPORT | wxLC_VIRTUAL),
        m_items(items) {
    AppendColumn("Index", wxLIST_FORMAT_LEFT, 140);
    AppendColumn("Value", wxLIST_FORMAT_LEFT, wxLIST_AUTOSIZE_USEHEADER);

    // This is the magic line: Tells the OS to allocate immediately for 266k
    // rows instantly
    SetItemCount(m_items.size());
  }

  // The OS requests text only for rows currently visible on screen
  wxString OnGetItemText(long item, long column) const override {
    if (item >= m_items.size())
      return "";
    if (column == 0)
      return wxString::Format("%ld", item);
    if (column == 1)
      return wxString::FromUTF8(m_items[item]);
    return "";
  }
};

GFrame::GFrame() : wxFrame(nullptr, wxID_ANY, "GGUF Viewer") {

  // Icons

#ifdef WIN32
  SetIcon(wxIcon("IDI_APP_ICON"));
#endif

  // Menus
  wxMenu *menuFile = new wxMenu;
  menuFile->Append(wxID_OPEN, "&Open...\tCtrl+O", "Open a file");
  menuFile->AppendSeparator();
  menuFile->Append(wxID_EXIT, "E&xit\tAlt+F4", "Quit the application");
  wxMenuBar *menuBar = new wxMenuBar;
  menuBar->Append(menuFile, "&File");
  SetMenuBar(menuBar);

  // Notebook
  notebook = new wxNotebook(this, wxID_ANY);

  // --- Tab 1: Metadata Table ---
  wxPanel *panelMetadata = new wxPanel(notebook, wxID_ANY);
  metadataTable = new wxDataViewListCtrl(panelMetadata, wxID_ANY);

  // Columns: Key, Type, Value
  metadataTable->AppendTextColumn("Key", wxDATAVIEW_CELL_INERT, 400);
  metadataTable->AppendTextColumn("Type", wxDATAVIEW_CELL_INERT, 400);
  metadataTable->AppendTextColumn("Value", wxDATAVIEW_CELL_INERT, 400);

  metadataTable->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED,
                      &GFrame::OnMetadataItemActivated, this);

  wxBoxSizer *sizerMeta = new wxBoxSizer(wxVERTICAL);
  sizerMeta->Add(metadataTable, 1, wxEXPAND | wxALL, 5);
  panelMetadata->SetSizer(sizerMeta);

  // --- Tab 2: Tensor Table ---
  wxPanel *panelTensors = new wxPanel(notebook, wxID_ANY);
  tensorTable = new wxDataViewListCtrl(panelTensors, wxID_ANY);

  // Columns: Name, Type, Shape, Element, Size(bytes)
  tensorTable->AppendTextColumn("Name", wxDATAVIEW_CELL_INERT, 400);
  tensorTable->AppendTextColumn("Type", wxDATAVIEW_CELL_INERT, 100);
  tensorTable->AppendTextColumn("Shape", wxDATAVIEW_CELL_INERT, 300);
  tensorTable->AppendTextColumn("Element", wxDATAVIEW_CELL_INERT, 150);
  tensorTable->AppendTextColumn("Size (bytes)", wxDATAVIEW_CELL_INERT, 150);

  wxBoxSizer *sizerTensor = new wxBoxSizer(wxVERTICAL);
  sizerTensor->Add(tensorTable, 1, wxEXPAND | wxALL, 5);
  panelTensors->SetSizer(sizerTensor);

  // Add into notebook tabs
  notebook->AddPage(panelMetadata, "Metadata");
  notebook->AddPage(panelTensors, "Tensors");

  metadataTable->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED,
                      &GFrame::OnMetadataItemActivated, this);

  metadataTable->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED,
                      &GFrame::OnMetadataSelectionChanged, this);
  CreateStatusBar(4);
  // -1 fills remaining space. The positive numbers lock the exact pixel sizes.
  int status_widths[] = {-1, 200, 200, 200};
  SetStatusWidths(4, status_widths);

  // Set clean placeholder baselines
  SetStatusText("Drop GGUF File", 0);
  SetStatusText("Version: -", 1);
  SetStatusText("Metadata: -", 2);
  SetStatusText("Tensors: -", 3);

  DragAcceptFiles(true);
}

void GFrame::OnDrop(wxDropFilesEvent &event) {
  int totalFiles = event.GetNumberOfFiles();
  if (totalFiles > 0) {
    wxString *files = event.GetFiles();
    wxString targetGguf = files[0]; // Focus on the first dropped file

    reader.clear();
    updateTables(targetGguf);
  }

  // 1. Skim the file using our persistent reader
}

void GFrame::OnMenu(wxCommandEvent &event) {
  switch (event.GetId()) {
  case wxID_OPEN: {
    wxFileDialog openFileDialog(
        this, _("Open GGUF Model File"), "", "",
        "GGUF files (*.gguf)|*.gguf|All files (*.*)|*.*",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (openFileDialog.ShowModal() == wxID_CANCEL) {
      break; // User canceled, break out of the switch cleanly
    }

    wxString filepath = openFileDialog.GetPath();

    reader.clear();
    updateTables(filepath);
    break;
  }

  case wxID_EXIT:
    Close(true);
    break;

  default:
    event.Skip();
    break;
  }
}

void GFrame::OnMetadataItemActivated(wxDataViewEvent &event) {
  int row = metadataTable->GetSelectedRow();
  if (row == wxNOT_FOUND)
    return;

  const auto &metadata_list = reader.get_metadata();
  if (row >= metadata_list.size())
    return;
  const auto &meta = metadata_list[row];

  // CASE 1: Open Array Viewer Window
  if (meta.is_array) {
    SetStatusText("Opening array inspection window...", 0);

    wxDialog dlg(this, wxID_ANY,
                 wxString::Format("Inspecting Array: %s", meta.key),
                 wxDefaultPosition, wxSize(700, 800),
                 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    GGUFVirtualListViewer *virtualList =
        new GGUFVirtualListViewer(&dlg, meta.array_items);
    sizer->Add(virtualList, 1, wxEXPAND | wxALL, 10);
    dlg.SetSizer(sizer);

    dlg.ShowModal();
    SetStatusText("Ready", 0);
  }
  // CASE 2: Open Text Preview Box Window (> 200 characters)
  else if (meta.type == GGUF_METADATA_VALUE_TYPE_STRING &&
           meta.value_str.length() > 200) {
    wxDialog dlg(this, wxID_ANY,
                 wxString::Format("Inspecting String Text: %s", meta.key),
                 wxDefaultPosition, wxSize(700, 800),
                 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    wxTextCtrl *textCtrl = new wxTextCtrl(
        &dlg, wxID_ANY, wxString::FromUTF8(meta.value_str), wxDefaultPosition,
        wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);

    sizer->Add(textCtrl, 1, wxEXPAND | wxALL, 10);
    dlg.SetSizer(sizer);

    dlg.ShowModal();
  }
}

void GFrame::OnMetadataSelectionChanged(wxDataViewEvent &event) {
  int row = metadataTable->GetSelectedRow();
  if (row == wxNOT_FOUND) {
    SetStatusText("Ready", 0);
    return;
  }

  const auto &metadata_list = reader.get_metadata();
  if (row < metadata_list.size()) {
    const auto &meta = metadata_list[row];

    if (meta.is_array) {
      SetStatusText("Double-click row to open array inspection panel window",
                    0);
    } else if (meta.type == GGUF_METADATA_VALUE_TYPE_STRING &&
               meta.value_str.length() > g_text_len) {
      SetStatusText(
          "Double-click row to view full extended string payload block", 0);
    } else {
      SetStatusText("Ready", 0);
    }
  }
}

void GFrame::updateTables(const wxString &filepath) {
  if (!reader.skim_file(filepath.ToStdString())) {
    SetStatusText("Error: Failed to parse GGUF file.", 0);
    SetStatusText("Version: -", 1);
    SetStatusText("Metadata: -", 2);
    SetStatusText("Tensors: -", 3);
    return;
  }

  // 2. Clear old data rows out of both tables completely
  metadataTable->DeleteAllItems();
  tensorTable->DeleteAllItems();

  for (const auto &meta : reader.get_metadata()) {
    wxVector<wxVariant> row;
    row.push_back(wxVariant(wxString::FromUTF8(meta.key)));

    if (meta.is_array)
      row.push_back(wxVariant(
          wxString::Format("Array%s", wxString::FromUTF8(meta.value_str))));
    else
      row.push_back(wxVariant(wxString(gguf_type_to_str(meta.type))));

    // Format Value Column: If it's an array, preview the first couple elements
    // inline
    if (meta.is_array) {
      wxString preview = "[";
      size_t limit = std::min(meta.array_items.size(), size_t(g_array_item_count));
      for (size_t i = 0; i < limit; ++i) {
        preview += wxString::FromUTF8(meta.array_items[i]);
        if (i < limit - 1)
          preview += ", ";
      }
      if (meta.array_items.size() > g_array_item_count)
        preview += ", ...";
      preview += "]";
      row.push_back(wxVariant(preview));
    } else {
      row.push_back(wxVariant(wxString::FromUTF8(meta.value_str)));
    }

    metadataTable->AppendItem(row);
  }

  for (const auto &tensor : reader.get_tensors()) {
    wxVector<wxVariant> row;
    row.push_back(wxVariant(wxString::FromUTF8(tensor.name)));

    row.push_back(wxVariant(wxString(ggml_type_to_str(tensor.type))));
    row.push_back(wxVariant(wxString(tensor.get_shape_str())));
    row.push_back(wxVariant(wxString::Format("%llu", tensor.element_count)));
    row.push_back(wxVariant(wxString::Format("%llu", tensor.size_bytes)));
    tensorTable->AppendItem(row);
  }

  notebook->SetSelection(0);

  // Field 0: Main File Status Message
  SetStatusText(
      wxString::Format("Loaded: %s", wxFileName(filepath).GetFullName()), 0);

  // Field 1: Format Layout Version
  SetStatusText(wxString::Format("GGUF v%u", reader.get_version()), 1);

  // Field 2: Total Metadata Records
  SetStatusText(
      wxString::Format("Metadata KVs: %zu", reader.get_metadata().size()), 2);

  // Field 3: Total Tensors Map
  SetStatusText(wxString::Format("Tensors: %zu", reader.get_tensors().size()),
                3);
}

// clang-format off
wxBEGIN_EVENT_TABLE(GFrame, wxFrame)
	EVT_MENU(wxID_ANY, GFrame::OnMenu) // Catches all menu clicks!
    EVT_DROP_FILES(GFrame::OnDrop) 
wxEND_EVENT_TABLE()

    // clang-format on