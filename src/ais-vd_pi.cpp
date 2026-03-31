/******************************************************************************
 * updated: 11-02-2014
 * Project:  OpenCPN
 * Purpose:  ais-vd Plugin
 * Author:   Dirk Smits
 *
 ***************************************************************************
 *   Copyright (C) 2014 by Dirk Smits                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
 ***************************************************************************
 */

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif  // precompiled headers

#include "ais-vd_pi.h"
#include "default_pi.xpm"
#include "wx/event.h"
#include "wx/tokenzr.h"

#include "nmea0183.h"

#define ANDROID_DIALOG_BACKGROUND_COLOR wxColour(_T("#7cb0e9"))
// the class factories, used to create and destroy instances of the PlugIn

extern "C" DECL_EXP opencpn_plugin* create_pi(void* ppimgr) {
  return new aisvd_pi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p) { delete p; }

//---------------------------------------------------------------------------------------------------------
//
//          PlugIn initialization and de-init
//
//---------------------------------------------------------------------------------------------------------

static wxBitmap load_plugin_icon(unsigned size) {
  wxBitmap bitmap;
#ifdef ocpnUSE_SVG
  wxLogDebug("Installing SVG icon");
  wxFileName fn;
  fn.SetPath(GetPluginDataDir("ais-vd_pi"));
  fn.AppendDir("data");
  fn.SetFullName("ais-vd.svg");
  bitmap = GetBitmapFromSVGFile(fn.GetFullPath(), size, size);
#else
  wxLogDebug("Installing default icon");
  bitmap = wxBitmap(default_pi);
#endif
  wxLogDebug("Installed icon, result: %s", bitmap.IsOk() ? "ok" : "fail");
  return bitmap;
}

aisvd_pi::aisvd_pi(void* ppimgr) : opencpn_plugin_118(ppimgr) {
  // Create the PlugIn icon
  wxInitAllImageHandlers();

  const static int ICON_SIZE = 48;  // FIXME: Needs size from GUI code
  m_plugin_icon = load_plugin_icon(ICON_SIZE);

  // Get and build if necessary a private data dir
  /*g_PrivateDataDir = *GetpPrivateApplicationDataLocation();
  g_PrivateDataDir += wxFileName::GetPathSeparator();
  g_PrivateDataDir += _T("ais-vd_pi");
  g_PrivateDataDir += wxFileName::GetPathSeparator();
  if (!::wxDirExists(g_PrivateDataDir))
    ::wxMkdir(g_PrivateDataDir);*/

  m_AIS_VoyDataWin = NULL;
  prefDlg = NULL;

  //    Get a pointer to the opencpn configuration object
  m_pconfig = GetOCPNConfigObject();
  //    And load the configuration items
  LoadConfig();
}

aisvd_pi::~aisvd_pi() {}

int aisvd_pi::Init(void) {
  AddLocaleCatalog(_T("opencpn-ais-vd_pi"));
  return (INSTALLS_TOOLBOX_PAGE | WANTS_PREFERENCES | WANTS_CONFIG);
}

bool aisvd_pi::DeInit(void) {
  // SaveConfig();

  if (m_AIS_VoyDataWin) {
    if (DeleteOptionsPage(aisvd_pi::m_AIS_VoyDataWin)) {
      m_AIS_VoyDataWin = NULL;
    } else {
      if (m_AIS_VoyDataWin) {  // Still alive?
        m_AIS_VoyDataWin = NULL;
        // wxLogMessage ("DeInit of ais-vd");
      }
    }
    if (prefDlg) {
      // delete prefDlg;
      prefDlg = NULL;
    }
  }
  return true;
}

int aisvd_pi::GetAPIVersionMajor() { return MY_API_VERSION_MAJOR; }

int aisvd_pi::GetAPIVersionMinor() { return MY_API_VERSION_MINOR; }

int aisvd_pi::GetPlugInVersionMajor() { return PLUGIN_VERSION_MAJOR; }

int aisvd_pi::GetPlugInVersionMinor() { return PLUGIN_VERSION_MINOR; }

wxBitmap* aisvd_pi::GetPlugInBitmap() { return &m_plugin_icon; }

wxString aisvd_pi::GetCommonName() { return _T("ais-vd"); }

wxString aisvd_pi::GetShortDescription() { return _T("AIS Voyage Data"); }

wxString aisvd_pi::GetLongDescription() {
  return _T("Set static voyage data to a AIS class A transceiver");
}

//void aisvd_pi::SetNMEASentence(wxString& sentence) {
//}

void PreferenceDlg::HandleAIVSD(ObservedEvt ev) {
  NMEA0183Id id("AIVSD");
  std::string payload = GetN0183Payload(id, ev);
  m_plugin.UpdateDataFromVSD(wxString::FromUTF8(payload.c_str()));
}

void aisvd_pi::ShowPreferencesDialog(wxWindow* parent) {
  wxString titleString = _("ais-vd_pi Preferences");

  long style = wxDEFAULT_DIALOG_STYLE;
#ifdef __WXOSX__
  style |= wxSTAY_ON_TOP;
#endif

  if (!prefDlg)
    prefDlg = new PreferenceDlg(parent, *this, wxID_ANY, titleString,
                                wxPoint(20, 20), wxDefaultSize, style);
  if (prefDlg->ShowModal() == wxID_OK)
    AIS_type =
        prefDlg->m_choice2->GetString(prefDlg->m_choice2->GetSelection());

  prefDlg->Hide();
  // delete prefDlg;
  // prefDlg = NULL;
}

void aisvd_pi::SetPluginMessage(wxString& message_id, wxString& message_body) {}
// Options Dialog Page management
void aisvd_pi::OnSetupOptions(void) {
  if (m_AIS_VoyDataWin) {
    // Already created? Should not be the case!
    wxLogMessage("m_AIS_VoyDataWin already created at setup! Set to NULL");
    m_AIS_VoyDataWin = NULL;
  }
  // Set validators
  const wxString AllowDest[] = {
      wxT("a"), wxT("b"), wxT("c"), wxT("d"), wxT("e"), wxT("f"), wxT("g"),
      wxT("h"), wxT("i"), wxT("j"), wxT("k"), wxT("l"), wxT("m"), wxT("n"),
      wxT("o"), wxT("p"), wxT("q"), wxT("r"), wxT("s"), wxT("t"), wxT("u"),
      wxT("v"), wxT("w"), wxT("x"), wxT("y"), wxT("z"), wxT("A"), wxT("B"),
      wxT("C"), wxT("D"), wxT("E"), wxT("F"), wxT("G"), wxT("H"), wxT("I"),
      wxT("J"), wxT("K"), wxT("L"), wxT("M"), wxT("N"), wxT("O"), wxT("P"),
      wxT("Q"), wxT("R"), wxT("S"), wxT("T"), wxT("U"), wxT("V"), wxT("W"),
      wxT("X"), wxT("Y"), wxT("Z"), wxT(" "), wxT("0"), wxT("1"), wxT("2"),
      wxT("3"), wxT("4"), wxT("5"), wxT("6"), wxT("7"), wxT("8"), wxT("9"),
      wxT(":"), wxT(";"), wxT("<"), wxT(">"), wxT("?"), wxT("@"), wxT("["),
      wxT("]"), wxT("!"), wxT(","), wxT("."), wxT("-"), wxT("="), wxT("*"),
      wxT("#")};
  wxArrayString* ArrayAllowDest = new wxArrayString(78, AllowDest);
  wxTextValidator DestVal(wxFILTER_INCLUDE_CHAR_LIST, &m_Destination);
  DestVal.SetIncludes(*ArrayAllowDest);
  ArrayAllowDest->Clear();
  // m_DestTextCtrl->SetValidator( DestVal);

  const wxString AllowDraught[] = {wxT("0"), wxT("1"), wxT("2"), wxT("3"),
                                   wxT("4"), wxT("5"), wxT("6"), wxT("7"),
                                   wxT("8"), wxT("9"), wxT(".")};
  wxArrayString* ArrayAllowDraught = new wxArrayString(11, AllowDraught);
  wxTextValidator DraughtVal(wxFILTER_INCLUDE_CHAR_LIST, &m_Draught);
  DraughtVal.SetIncludes(*ArrayAllowDraught);
  ArrayAllowDraught->Clear();
  // DraughtTextCtrl->SetValidator( DraughtVal );

  const wxString AllowPersons[] = {wxT("0"), wxT("1"), wxT("2"), wxT("3"),
                                   wxT("4"), wxT("5"), wxT("6"), wxT("7"),
                                   wxT("8"), wxT("9")};
  wxArrayString* ArrayAllowPersons = new wxArrayString(10, AllowPersons);
  wxTextValidator PersonsVal(wxFILTER_INCLUDE_CHAR_LIST, &m_Persons);
  PersonsVal.SetIncludes(*ArrayAllowPersons);
  ArrayAllowPersons->Clear();
  // PersonsTextCtrl->SetValidator( PersonsVal );

  //  Create the AISVD Options panel, and load it
  m_AIS_VoyDataWin =
      AddOptionsPage(PI_OPTIONS_PARENT_SHIPS, _("AIS Voyage data"));
  if (!m_AIS_VoyDataWin) {
    wxLogMessage(_T("Error: OnSetupOptions AddOptionsPage failed!"));
    return;
  }
  if (!prefDlg) {
    // Start hidden to run ObservableListener who need a Dlg
    prefDlg = new PreferenceDlg(m_AIS_VoyDataWin, *this, wxID_ANY,
                                "ais-vd_pi Preferences", wxPoint(20, 20),
                                wxDefaultSize, wxDEFAULT_DIALOG_STYLE);
  }

  wxStaticBox* itemStaticBoxSizer3Static =
      new wxStaticBox(m_AIS_VoyDataWin, wxID_ANY,
                      _("Set static voyage data on the AIS class A"));
  wxStaticBoxSizer* itemStaticBoxSizer3 =
      new wxStaticBoxSizer(itemStaticBoxSizer3Static, wxVERTICAL);
  m_AIS_VoyDataWin->SetSizer(itemStaticBoxSizer3);

  wxFlexGridSizer* itemFlexGridSizer4 = new wxFlexGridSizer(2);
  itemStaticBoxSizer3->Add(itemFlexGridSizer4, 0, wxGROW | wxALL, 20);

  wxStaticText* itemStaticText5 =
      new wxStaticText(m_AIS_VoyDataWin, wxID_STATIC, _("Navigational status"),
                       wxDefaultPosition, wxDefaultSize, 0);
  itemFlexGridSizer4->Add(itemStaticText5, 0,
                          wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 5);

  StatusChoiceStrings.Add(_("Underway using engine"));
  StatusChoiceStrings.Add(_("At anchor"));
  StatusChoiceStrings.Add(_("Not under command"));
  StatusChoiceStrings.Add(_("Restricted manoeuverability"));
  StatusChoiceStrings.Add(_("Constrained by draught"));
  StatusChoiceStrings.Add(_("Moored"));
  StatusChoiceStrings.Add(_("Aground"));
  StatusChoiceStrings.Add(_("Engaged in Fishing"));
  StatusChoiceStrings.Add(_("Under way sailing"));

  StatusChoice = new wxChoice(m_AIS_VoyDataWin, ID_CHOICE, wxDefaultPosition,
                              wxDefaultSize, StatusChoiceStrings, 0);
  itemFlexGridSizer4->Add(StatusChoice, 0,
                          wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 5);
  StatusChoice->Bind(wxEVT_CHOICE,
                     [&](wxCommandEvent& ev) { OnNavStatusSelect(ev); });


  wxStaticText* itemStaticText7 =
      new wxStaticText(m_AIS_VoyDataWin, wxID_STATIC, _("Enter destination"),
                       wxDefaultPosition, wxDefaultSize, 0);
  itemFlexGridSizer4->Add(itemStaticText7, 0,
                          wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 5);

  wxStaticText* itemStaticText19 = new wxStaticText(
      m_AIS_VoyDataWin, wxID_STATIC, _("or select a previous used"),
      wxDefaultPosition, wxDefaultSize, 0);
  itemFlexGridSizer4->Add(itemStaticText19, 0,
                          wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 5);

  m_DestTextCtrl = new wxTextCtrl(m_AIS_VoyDataWin, ID_TEXTCTRL, wxEmptyString,
                                  wxDefaultPosition, wxDefaultSize, 0, DestVal);
  m_DestTextCtrl->SetMaxLength(20);
  itemFlexGridSizer4->Add(m_DestTextCtrl, 0,
                          wxGROW | wxALIGN_CENTER_VERTICAL | wxALL, 5);
  m_DestTextCtrl->Bind(wxEVT_CHAR,
                       [&](wxKeyEvent& ev) { OnAnyValueChange(ev); });


  m_DestComboBox = new wxComboBox(m_AIS_VoyDataWin, ID_COMBCTRL, wxEmptyString,
                                  wxDefaultPosition, wxDefaultSize, 0, NULL, 0);
  itemFlexGridSizer4->Add(m_DestComboBox, 0, wxEXPAND | wxALL, 5);
  m_DestComboBox->Bind(wxEVT_COMBOBOX,
                       [&](wxCommandEvent& ev) { OnDestValSelect(ev); });
  wxStaticText* itemStaticText9 =
      new wxStaticText(m_AIS_VoyDataWin, wxID_STATIC, _("Draught (m)"),
                       wxDefaultPosition, wxDefaultSize, 0);
  itemFlexGridSizer4->Add(itemStaticText9, 0,
                          wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 5);

  DraughtTextCtrl =
      new wxTextCtrl(m_AIS_VoyDataWin, ID_TEXTCTRL1, wxEmptyString,
                     wxDefaultPosition, wxDefaultSize, 0, DraughtVal);
  itemFlexGridSizer4->Add(DraughtTextCtrl, 0,
                          wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 5);
  DraughtTextCtrl->Bind(wxEVT_CHAR,
                        [&](wxKeyEvent& ev) { OnAnyValueChange(ev); });

  wxStaticText* itemStaticText11 = new wxStaticText(
      m_AIS_VoyDataWin, wxID_STATIC, _("No. of Persons onboard"),
      wxDefaultPosition, wxDefaultSize, 0);
  itemFlexGridSizer4->Add(itemStaticText11, 0,
                          wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 5);

  PersonsTextCtrl =
      new wxTextCtrl(m_AIS_VoyDataWin, ID_TEXTCTRL2, wxEmptyString,
                     wxDefaultPosition, wxDefaultSize, 0, PersonsVal);
  itemFlexGridSizer4->Add(PersonsTextCtrl, 0,
                          wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 5);
  PersonsTextCtrl->Bind(wxEVT_CHAR,
                        [&](wxKeyEvent& ev) { OnAnyValueChange(ev); });

  //    Dummy new row
  wxStaticText* itemStaticText20 = new wxStaticText(m_AIS_VoyDataWin,
    wxID_STATIC, _(""),      wxDefaultPosition, wxDefaultSize, 0);
  itemFlexGridSizer4->Add(itemStaticText20, 0,
                          wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 0);
  wxStaticText* itemStaticText21 =
      new wxStaticText(m_AIS_VoyDataWin, wxID_STATIC, _(""), wxDefaultPosition,
                       wxDefaultSize, 0);
  itemFlexGridSizer4->Add(itemStaticText21, 0,
                          wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 0);

  m_cbUseRegionalappl = new wxCheckBox(m_AIS_VoyDataWin, ID_TEXTCTRL2,
    _("Control Inland Blue Sign"),wxDefaultPosition, wxDefaultSize,0);
  m_cbUseRegionalappl->SetValue(false);
  itemFlexGridSizer4->Add(m_cbUseRegionalappl, 0,
    wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 5);
  m_cbUseRegionalappl->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent& ev) { OnUseBlueSignControl(ev); });

  // For now hide the blue sign option.
  m_cbUseRegionalappl->Show(false);

  wxString m_rbBlueSignOptions[] = {
      _("Not used"), _("Set not active"), _("Set active")};
  m_rbBlueSignStatus = new wxRadioBox(
      m_AIS_VoyDataWin, ID_TEXTCTRL2, (""), wxDefaultPosition,
      wxDefaultSize, 3, m_rbBlueSignOptions, 0, 0);
  itemFlexGridSizer4->Add(m_rbBlueSignStatus, 0,
                          wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 1);
  m_rbBlueSignStatus->Show(false);
  m_rbBlueSignStatus->Bind(wxEVT_RADIOBOX,
                            [&](wxCommandEvent& ev) { OnAnyValueChange(ev); });

  wxStaticBoxSizer* ETAbox = new wxStaticBoxSizer(
      new wxStaticBox(m_AIS_VoyDataWin, wxID_ANY, _("E.T.A. Date and Time")),
      wxVERTICAL);

  wxFlexGridSizer* EtaFlexgrid = new wxFlexGridSizer(4, 5, 5);
  // EtaFlexgrid->AddGrowableCol(3);

  // Set defaults. Normally later updated by a AIS.
  wxDateTime now = wxDateTime::Now().MakeUTC();
  unsigned short EtaInitMonth = now.GetMonth() + 1;  // wx months start at 0!
  unsigned short EtaInitDay = now.GetDay();
  unsigned short EtaInitHour = now.GetHour();

  wxStaticText* monthtext =
      new wxStaticText(m_AIS_VoyDataWin, wxID_STATIC, _("Month"));
  EtaFlexgrid->Add(monthtext, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL, 5);

  m_pCtrlMonth = new wxSpinCtrl(m_AIS_VoyDataWin, wxID_ANY, wxEmptyString,
                                wxDefaultPosition, wxDefaultSize,
                                wxSP_ARROW_KEYS, 1, 12, EtaInitMonth);
  EtaFlexgrid->Add(m_pCtrlMonth, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 0);
  m_pCtrlMonth->Bind(wxEVT_SPINCTRL,
                     [&](wxCommandEvent& ev) { OnMonthChange(ev); });


  wxStaticText* daytext =
      new wxStaticText(m_AIS_VoyDataWin, wxID_STATIC, _("Day"));
  EtaFlexgrid->Add(daytext, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL, 5);

  m_pCtrlDay = new wxSpinCtrl(m_AIS_VoyDataWin, wxID_ANY, wxEmptyString,
                              wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS,
                              1, 31, EtaInitDay);
  EtaFlexgrid->Add(m_pCtrlDay, 0, wxALIGN_LEFT | wxEXPAND | wxALL, 0);
  m_pCtrlDay->Bind(wxEVT_SPINCTRL,
                   [&](wxCommandEvent& ev) { OnAnyValueChange(ev); });

  wxStaticText* hourtext =
      new wxStaticText(m_AIS_VoyDataWin, wxID_STATIC, _("Hour (UTC)"));
  EtaFlexgrid->Add(hourtext, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL, 5);

  m_pCtrlHour = new wxSpinCtrl(m_AIS_VoyDataWin, wxID_ANY, wxEmptyString,
                               wxDefaultPosition, wxDefaultSize,
                               wxSP_ARROW_KEYS, 0, 23, EtaInitHour);
  EtaFlexgrid->Add(m_pCtrlHour, 0, wxALIGN_CENTER_VERTICAL, 0);
  m_pCtrlHour->Bind(wxEVT_SPINCTRL,
                    [&](wxCommandEvent& ev) { OnAnyValueChange(ev); });

  wxStaticText* minutetext =
      new wxStaticText(m_AIS_VoyDataWin, wxID_STATIC, _("Minute"));
  EtaFlexgrid->Add(minutetext, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL, 5);

  m_pCtrlMinute = new wxSpinCtrl(m_AIS_VoyDataWin, wxID_ANY, wxEmptyString,
                                 wxDefaultPosition, wxDefaultSize,
                                 wxSP_ARROW_KEYS, 0, 59, 30);
  EtaFlexgrid->Add(m_pCtrlMinute, 0, wxEXPAND | wxALL, 0);
  m_pCtrlMinute->Bind(wxEVT_SPINCTRL,
                      [&](wxCommandEvent& ev) { OnAnyValueChange(ev); });

  ETAbox->Add(EtaFlexgrid, 0, wxEXPAND, 20);

  itemStaticBoxSizer3->Add(ETAbox, 0, wxEXPAND, 20);

  wxBoxSizer* itemBoxSizer1 = new wxBoxSizer(wxHORIZONTAL);
  itemStaticBoxSizer3->Add(itemBoxSizer1, 0, wxGROW | wxALL, 5);

  // Read from AIS button
  m_BtnReadAIS = new wxButton(m_AIS_VoyDataWin, ID_BUTTON, _T("Read from AIS"),
                              wxDefaultPosition, wxDefaultSize, 0);
  itemBoxSizer1->Add(m_BtnReadAIS, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
  m_BtnReadAIS->Bind(wxEVT_BUTTON,
                     [&](wxCommandEvent& ev) { OnReadBtnClick(ev); });

  // Send button and AIS reply info text
  //  Deafult user message if no reply from AIS
  wxString defmsg = _("Yet no answer from any AIS!");
  defmsg.Append("\n" + _("Please check OCPN connections and AIS cabling."));
  defmsg.Append("\n" + _("See Preferences for more info."));
  m_SendBtn = new wxButton(m_AIS_VoyDataWin, ID_BUTTON1, defmsg,
                           wxDefaultPosition, wxDefaultSize, 0);
  itemBoxSizer1->Add(m_SendBtn, -1, wxEXPAND | wxALL, 5);
  m_SendBtn->Bind(wxEVT_BUTTON,
                  [&](wxCommandEvent& ev) { OnSendBtnClick(ev); });

  // Update values in Controls
  StatusChoice->SetStringSelection(StatusChoiceStrings[0]);
  m_DestTextCtrl->SetValue(m_Destination);
  DraughtTextCtrl->SetValue(m_Draught);
  PersonsTextCtrl->SetValue(m_Persons);
  m_DestComboBox->Append(_("Drop down to select"));
  wxStringTokenizer tkn(m_InitDest, ";");
  while (tkn.HasMoreTokens()) {
    m_DestComboBox->Append(tkn.GetNextToken());
  }
  m_DestComboBox->Select(0);

  SetMaxDay();

  // Uppdate data from AIS if available
  RequestAISstatus();
  // content construction
  m_AIS_VoyDataWin->Layout();
}

bool aisvd_pi::LoadConfig(void) {
  wxFileConfig* pConf = (wxFileConfig*)m_pconfig;

  if (pConf) {
    wxString temp;
    pConf->SetPath(_T("/PlugIns/ais-vd"));
    pConf->Read(_T("Destination"), &m_Destination);
    pConf->Read(_T("Draught"), &m_Draught, wxEmptyString);
    pConf->Read(_T("Persons"), &m_Persons, wxEmptyString);
    pConf->Read(_T("DestSelections"), &m_InitDest);
  }

  return true;
}

bool aisvd_pi::SaveConfig(void) {
  wxFileConfig* pConf = (wxFileConfig*)m_pconfig;

  if (pConf) {
    pConf->SetPath(_T("/PlugIns/ais-vd"));
    pConf->Write(_T("Destination"), m_Destination);
    pConf->Write(_T("Draught"), m_Draught);
    pConf->Write(_T("Persons"), m_Persons);
    // Save max 18 dest-selections to config.
    if (m_AIS_VoyDataWin) {
      wxString destarr;
      size_t size(0);
      size = wxMin(18, m_DestComboBox->GetCount());
      for (size_t i = 1; i < size; i++) {
        m_DestComboBox->Select(i);
        destarr << m_DestComboBox->GetValue();
        if (i < size - 1) destarr << ";";
      }
      m_DestComboBox->Select(0);
      if (size) pConf->Write(_T("DestSelections"), destarr);
    }
  }
  return true;
}

void aisvd_pi::OnCloseToolboxPanel(int page_sel, int ok_apply_cancel) {}

void aisvd_pi::UpdateDestVal() {
  if (m_DestComboBox->GetValue() != wxEmptyString &&
      m_DestComboBox->GetSelection() != 0) {
    m_Destination = m_DestComboBox->GetValue();
    // Move last selection to top
    int pos = m_DestComboBox->GetSelection();
    if (pos > 2) {
      m_DestComboBox->Delete(pos);
      m_DestComboBox->Insert(m_Destination, 1);
    }
    m_DestComboBox->Select(0);
  } else if (m_DestTextCtrl->GetValue() != wxEmptyString) {
    m_Destination = m_DestTextCtrl->GetValue();
  } else
    return;

  m_Destination = m_Destination.MakeUpper();
  m_DestTextCtrl->SetValue(m_Destination);
  // Check if already exist else add it
  if (wxNOT_FOUND == m_DestComboBox->FindString(m_Destination)) {
    m_DestComboBox->Insert(m_Destination, 1);
  }
}
void aisvd_pi::UpdateDraught() {
  m_Draught = DraughtTextCtrl->GetValue();
  // Don't change if user didn't edit
  if (m_Draught != wxEmptyString) {
    double temp;
    m_Draught.ToDouble(&temp);
    if (temp >= 25.5) temp = 25.5;
    m_Draught = wxString::Format(wxT("%2.1f"), temp);
    DraughtTextCtrl->ChangeValue(m_Draught);
  }
}
void aisvd_pi::UpdatePersons() {
  // Don't change if user didn't edit
  m_Persons = PersonsTextCtrl->GetValue();
  if (m_Persons != wxEmptyString) {
    long temp;
    m_Persons.ToLong(&temp);
    if (temp >= 8100) temp = 8100;
    m_Persons = wxString::Format(_T("%1d"), temp);
    PersonsTextCtrl->ChangeValue(m_Persons);
  }
}
// Set Max day according to month
void aisvd_pi::SetMaxDay() {
  int month = m_pCtrlMonth->GetValue();
  int max = 31;
  switch (month) {
    case 2: {
      max = 29;
      break;
    }
    case 4:
    case 6:
    case 9:
    case 11: {
      max = 30;
      break;
    }
    default:
      max = 31;
  }
  m_pCtrlDay->SetRange(1, max);
}

void aisvd_pi::SendSentence() {
  bool psw = false;
  // $IISPW,E,1,00000000,,,0*10  for VSD? A possible password.
  // ref: http://annoyingdesigns.com/vsd/VSDControl.pdf
  //$RASPW,EPV,503123450,1,SESAME*hh
  //$IISPW,VSD,00000000,1,303030 ??
  /*$--SPW, ccc, c--c, x, c--c*hh<CR><LF>
  Password 4
  Password level 3
  Unique identifier 2
  Password protected sentence 1
  Notes:
    1) The following sentence formatter that should be protected ( for example
  EPV ) 2) For AIS the unique identifier is the MMSI. 3) An integer number as
  defined below : 1 = User level password; 2 = Administrator level password;
    3 - 9 = Reserved
    4) Password as text up to 32 characters.*/

  if (psw) {
    wxString S;
    S = _T("$ECSPW");  // EC for Electronic Chart
    S.Append(_T(","));
    S.Append(_T("E"));
    S.Append(_T(","));
    S.Append(_T("303030"));
    S.Append(_T(","));
    S.Append(_T(","));
    S.Append(_T(","));
    S.Append(_T("0"));
    S.Append(_T("*"));  // End data
    S.Append(wxString::Format(_T("%02X"), ComputeChecksum(S)));
    S += _T("\r\n");
    // wxPuts(S);
    PushNMEABuffer(S);  // finaly send the password string
  }

  wxString S;
  S = _T("$ECVSD,");  // EC for Electronic Chart
  // We dont send ship type. It will be set by AIS static
  // data and can be password protected by some devices
  S.Append(_T(","));
  S.Append(m_Draught);
  S.Append(_T(","));
  S.Append(m_Persons);
  S.Append(_T(","));
  S.Append(m_Destination);
  S.Append(_T(","));
  S.Append(
      wxString::Format(_T("%02d%02d00,"), aisvd_pi::m_pCtrlHour->GetValue(),
                       aisvd_pi::m_pCtrlMinute->GetValue()));  // eta time HHmm
  S.Append(wxString::Format(_T("%02d,"),
                            aisvd_pi::m_pCtrlDay->GetValue()));  // eta Day
  S.Append(wxString::Format(_T("%02d,"),
                            aisvd_pi::m_pCtrlMonth->GetValue()));  // eta Month
  S.Append(wxString::Format(
      _T("%d,"), StatusChoice->GetSelection()));  // Navigation status
  if (m_cbUseRegionalappl->GetValue()) // Else NULL: Not change use of local HW
    S.Append(wxString::Format(_T("%d"), m_rbBlueSignStatus->GetSelection()));
  S.Append(_T("*"));              // End data
  S.Append(wxString::Format(_T("%02X"), ComputeChecksum(S)));
  S += _T("\r\n");
  // wxPuts(S);
  PushNMEABuffer(S);  // finaly send NMEA string
  // Now querry a AIS for updated voyage data
  RequestAISstatus();
}

void aisvd_pi::RequestAISstatus() {
  // Deafult user message if no reply from AIS
  wxString msg = _("Yet no answer from any AIS!");
  msg.Append("\n" + _("Please check OCPN connections and AIS cabling."));
  msg.Append("\n" + _("See Preferences for more info."));

  m_SendBtn->SetLabel(msg);
  m_AIS_VoyDataWin->Layout();

  wxString S;
  S = _T("$ECAIQ");  // EC for Electronic Chart
  S.Append(_T(","));
  S.Append(_T("VSD"));
  S.Append(_T("*"));
  S.Append(wxString::Format(_T("%02X"), ComputeChecksum(S)));
  S += _T("\r\n");
  PushNMEABuffer(S);
}

void aisvd_pi::UpdateDataFromVSD(const wxString& sentence) {
  //     VSD, x.x, x.x, x.x, c c, hhmmss.ss, xx, xx, x.x, x.x*hh
  //           1    2    3    4      5       6    7   8    9
  //  9)Regional application flags, 0 to 15
  //  8)Navigational status
  //  7)Estimated month of arrival at destination, 00 to 12
  //  6)Estimated day of arrival at destination, 00 to 31
  //  5)Estimated UTC of arrival at destination
  //  4)Destination, 1 - 20 characters
  //  3)Persons on board, 0 to 8 191
  //  2)Maximum present static draught, 0 to 25, 5 m
  //  1) Type of ship and cargo category, 0 to 255

  wxString msg = _("Reply from AIS");
  wxDateTime now = wxDateTime::Now();  // .MakeUTC();
  msg.Append(
      wxString::Format(_T(" (%02d:%02d) : "), now.GetHour(), now.GetMinute()));
  wxString nmea = sentence.Mid(0, sentence.Len() - 2);
  nmea.Replace("*", ",");  //   Token fails on "*"
  // Create an understandable user message
  wxString VSD_Nr[11];
  int nr = 0;
  wxStringTokenizer tkn(nmea, ",");
  while (tkn.HasMoreTokens()) {
    VSD_Nr[nr] = (tkn.GetNextToken());
    nr += 1;  // xxVSD
    if (nr > 10) break;
  }

  int id = wxAtoi(VSD_Nr[1]);
  msg.Append("  (" + GetShipType(id) + ")\n");

  int statusNr = wxAtoi(VSD_Nr[8]);
  wxString status = StatusChoiceStrings[statusNr];
  StatusChoice->SetStringSelection(StatusChoiceStrings[statusNr]);
  // Clean out possible white space complements in destination
  wxString dest = VSD_Nr[4];
  dest.Replace(("  "), wxEmptyString);
  msg.Append("Values in controls are now updated by AIS status");
  wxString hour = VSD_Nr[5].Mid(0, 2);
  wxString minutes = VSD_Nr[5].Mid(2, 2);
  m_SendBtn->SetLabel(msg);
  // Upptade controls
  m_DestTextCtrl->ChangeValue(dest);
  m_pCtrlMonth->SetValue(wxAtoi(VSD_Nr[7]));
  m_pCtrlDay->SetValue(wxAtoi(VSD_Nr[6]));
  m_pCtrlHour->SetValue(wxAtoi(hour));
  m_pCtrlMinute->SetValue(wxAtoi(minutes));
  DraughtTextCtrl->ChangeValue(VSD_Nr[2]);
  if (VSD_Nr[3] != _T("0") && VSD_Nr[3] != wxEmptyString) {
    PersonsTextCtrl->ChangeValue(VSD_Nr[3]);
  }
  m_rbBlueSignStatus->SetSelection(wxAtoi(VSD_Nr[9]));
  SetMaxDay();
  m_AIS_VoyDataWin->Layout();
}

void aisvd_pi::SetSendBtnLabel() {
  m_SendBtn->SetLabel(_("Send to AIS"));
  m_AIS_VoyDataWin->Layout();
}

unsigned char aisvd_pi::ComputeChecksum(wxString sentence) const {
  unsigned char calculated_checksum = 0;
  for (wxString::const_iterator i = sentence.begin() + 1;
       i != sentence.end() && *i != '*'; ++i)
    calculated_checksum ^= static_cast<unsigned char>(*i);

  return (calculated_checksum);
}

// The preference dialog
PreferenceDlg::PreferenceDlg(wxWindow* parent, aisvd_pi& plugin, wxWindowID id,
                             const wxString& title, const wxPoint& pos,
                             const wxSize& size, long style)
    : wxDialog(parent, id, title, pos, size, style), m_plugin(plugin) {
#ifdef __OCPN__ANDROID__
  SetBackgroundColour(ANDROID_DIALOG_BACKGROUND_COLOR);
#endif

  this->SetSizeHints(wxDefaultSize, wxDefaultSize);

  wxBoxSizer* bSizer2;
  bSizer2 = new wxBoxSizer(wxVERTICAL);

  // Plugin Version
  wxString extVersion;
  extVersion.Printf(_T("%d.%d.%d"), PLUGIN_VERSION_MAJOR, PLUGIN_VERSION_MINOR,
                    PLUGIN_VERSION_PATCH);

  wxString versionText = _("Plugin version: ") + extVersion;
  wxStaticText* versionTextBox = new wxStaticText(this, wxID_ANY, versionText);
  bSizer2->Add(versionTextBox, 0, wxALL, 20);

  wxString helptxt = _("OpenCPN connections help:");
  helptxt.Append(_T("\n"));
  helptxt.Append(
      _("You need both input and output connections for the AIS device."));
  helptxt.Append("\n" +
                 _("To minimize traffic, connection filters may be feasible:"));
  helptxt.Append("\n" + _("For the output filter transmit only VSD and AIQ"));
  helptxt.Append(
      "\n" + _("For a possible input filter we need at least VSD (and AIVDM + "
               "GNSS?)"));

  m_staticTexthelp = new wxStaticText(this, wxID_ANY, helptxt,
                                      wxDefaultPosition, wxDefaultSize, 0);
  bSizer2->Add(m_staticTexthelp, 0, wxALL, 20);

  wxGridSizer* gSizer2;
  gSizer2 = new wxGridSizer(2);

  m_staticText2 = new wxStaticText(
      this, wxID_ANY,
      wxT("Type of AIS. So far no options. The future may change that?"),
      wxDefaultPosition, wxDefaultSize, 0);
  // m_staticText2->Wrap( -1 );
  gSizer2->Add(m_staticText2, 0, wxALL, 20);

  wxString m_choice2Choices[] = {
      wxT("Class A Transponder using NMEA0183 $ECVSD")};
  int m_choice2NChoices = sizeof(m_choice2Choices) / sizeof(wxString);
  m_choice2 = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                           m_choice2NChoices, m_choice2Choices, 0);
  m_choice2->SetSelection(0);
  gSizer2->Add(m_choice2, 0, wxALL, 20);

  bSizer2->Add(gSizer2, 0, 0, 20);

  m_sdbSizer2 = new wxStdDialogButtonSizer();
  m_sdbSizer2OK = new wxButton(this, wxID_OK);
  m_sdbSizer2->AddButton(m_sdbSizer2OK);
  m_sdbSizer2Cancel = new wxButton(this, wxID_CANCEL);
  m_sdbSizer2->AddButton(m_sdbSizer2Cancel);
  m_sdbSizer2->Realize();

  bSizer2->Add(m_sdbSizer2, 1, wxEXPAND, 20);

  this->SetSizer(bSizer2);
  this->Layout();
  bSizer2->Fit(this);

  this->Centre(wxBOTH);

  NMEA0183Id nmea_id("AIVSD");
  wxDEFINE_EVENT(EVT_AIVSD, ObservedEvt);
  aivsd_listener = GetListener(nmea_id, EVT_AIVSD, this);
  Bind(EVT_AIVSD, [&](ObservedEvt ev) { HandleAIVSD(ev); });
}

PreferenceDlg::~PreferenceDlg() {}

void aisvd_pi::OnReadBtnClick(wxCommandEvent& event) {
  RequestAISstatus();
  event.Skip();
}

void aisvd_pi::OnSendBtnClick(wxCommandEvent& event) {
  UpdateDestVal();
  UpdateDraught();
  UpdatePersons();
  SaveConfig();
  SendSentence();
  event.Skip();
}

void aisvd_pi::OnDestValSelect(wxCommandEvent& event) {
  SetSendBtnLabel();
  UpdateDestVal();
  event.Skip();
}

void aisvd_pi::OnAnyValueChange(wxCommandEvent& event) {
  SetSendBtnLabel();
  event.Skip();
}

void aisvd_pi::OnAnyValueChange(wxKeyEvent& event) {
  SetSendBtnLabel();
  event.Skip();
}


void aisvd_pi::OnMonthChange(wxCommandEvent& event) {
  SetMaxDay();
  SetSendBtnLabel();
  event.Skip();
}

void aisvd_pi::OnNavStatusSelect(wxCommandEvent& event) {
  SetSendBtnLabel();
  event.Skip();
}

void aisvd_pi::OnUseBlueSignControl(wxCommandEvent& event) {
  if (m_cbUseRegionalappl->GetValue()) m_rbBlueSignStatus->Show(true);
  else m_rbBlueSignStatus->Show(false);
  SetSendBtnLabel();
  event.Skip();
}

wxString aisvd_pi::GetShipType(int id) {
  switch (id) {
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
      return _("Wing in ground (WIG)");
    case 30:
      return _("Fishing");
    case 31:
      return _("Towing");
    case 32:
      return _("Towing (Long)");
    case 33:
      return _("Dredging or underwater ops");
    case 34:
      return _("Diving ops");
    case 35:
      return _("Military ops");
    case 36:
      return _("Sailing vessel");
    case 37:
      return _("Pleasure Craft");
    case 40:
    case 41:
    case 42:
    case 43:
    case 44:
      return _("High speed craft (HSC)");
    case 50:
      return _("Pilot Vessel");
    case 51:
      return _("Search and Rescue vessel");
    case 52:
      return _("Tug");
    case 53:
      return _("Port Tender");
    case 55:
      return _("Law Enforcement");
    case 58:
      return _("Medical Transport");
    case 60:
    case 61:
    case 62:
    case 63:
    case 64:
      return _("Passenger ship");
    case 70:
    case 71:
    case 72:
    case 73:
    case 74:
    case 79:
      return _("Cargo ship");
    case 80:
    case 81:
    case 82:
    case 83:
    case 84:
      return _("Tanker");
    default:
      return _("Other");
  }
}
