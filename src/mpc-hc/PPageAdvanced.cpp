/*
* (C) 2014-2017 see Authors.txt
*
* This file is part of MPC-HC.
*
* MPC-HC is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* MPC-HC is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/

#include "stdafx.h"
#include "PPageAdvanced.h"
#include "mplayerc.h"
#include "MainFrm.h"
#include "EventDispatcher.h"
#include <strsafe.h>
#include "CrashReporter.h"
#include "ExceptionHandler.h"

CPPageAdvanced::CPPageAdvanced()
    : CMPCThemePPageBase(IDD, IDD)
{
    EventRouter::EventSelection fires;
    fires.insert(MpcEvent::DEFAULT_TOOLBAR_SIZE_CHANGED);
    GetEventd().Connect(m_eventc, fires);
}

void CPPageAdvanced::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST1, m_list);
    DDX_Control(pDX, IDC_COMBO1, m_comboBox);
    DDX_Control(pDX, IDC_SPIN1, m_spinButtonCtrl);
}

BOOL CPPageAdvanced::OnInitDialog()
{
    __super::OnInitDialog();

    if (CFont* pFont = m_list.GetFont()) {
        if (!m_fontBold.m_hObject) {
            LOGFONT logfont;
            pFont->GetLogFont(&logfont);
            logfont.lfWeight = FW_BOLD;
            m_fontBold.CreateFontIndirect(&logfont);
        }
    }

    SetRedraw(FALSE);
    m_list.SetExtendedStyle(m_list.GetExtendedStyle() /* | LVS_EX_FULLROWSELECT */ | LVS_EX_AUTOSIZECOLUMNS /*| LVS_EX_DOUBLEBUFFER */ | LVS_EX_INFOTIP);
    m_list.setAdditionalStyles(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);
    m_list.InsertColumn(COL_NAME, ResStr(IDS_PPAGEADVANCED_COL_NAME), LVCFMT_LEFT);
    m_list.InsertColumn(COL_VALUE, ResStr(IDS_PPAGEADVANCED_COL_VALUE), LVCFMT_LEFT);

    if (auto pToolTip = m_list.GetToolTips()) {
        // Set topmost for tooltip window. Workaround bug https://connect.microsoft.com/VisualStudio/feedback/details/272350
        pToolTip->SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOREDRAW | SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOOWNERZORDER);
    }

    GetDlgItem(IDC_EDIT1)->GetWindowRect(editRect);
    ScreenToClient(editRect);

    m_spinButtonCtrl.SetBuddy(GetDlgItem(IDC_EDIT1));

    GetDlgItem(IDC_EDIT1)->ShowWindow(SW_HIDE);
    GetDlgItem(IDC_COMBO1)->ShowWindow(SW_HIDE);
    GetDlgItem(IDC_RADIO1)->ShowWindow(SW_HIDE);
    GetDlgItem(IDC_RADIO2)->ShowWindow(SW_HIDE);
    GetDlgItem(IDC_BUTTON1)->ShowWindow(SW_HIDE);
    GetDlgItem(IDC_SPIN1)->ShowWindow(SW_HIDE);

    GetDlgItemText(IDC_RADIO1, m_strTrue);
    GetDlgItemText(IDC_RADIO2, m_strFalse);

    InitSettings();

    m_list.SetColumnWidth(1, LVSCW_AUTOSIZE_USEHEADER);
    m_list.SetColumnWidth(0, LVSCW_AUTOSIZE_USEHEADER);

    SetRedraw(TRUE);
    return TRUE;
}

void CPPageAdvanced::InitSettings()
{
    auto& s = AfxGetAppSettings();

    auto addBoolItem = [this](int nItem, CString name, bool defaultValue, bool & settingReference, CString toolTipText) {
        auto pItem = std::make_shared<SettingsBool>(name, defaultValue, settingReference, toolTipText);
        auto eSetting = static_cast<ADVANCED_SETTINGS>(nItem);
        int iItem = m_list.InsertItem(nItem, pItem->GetName());
        m_hiddenOptions[eSetting] = pItem;
        m_list.setItemTextWithDefaultFlag(iItem, COL_VALUE, (pItem->GetValue() ? m_strTrue : m_strFalse), !IsDefault(eSetting));
        m_list.SetItemData(iItem, nItem);
    };

    // The range parameter defines range (inclusive) that particular option can have.
    auto addIntItem = [this](int nItem, CString name, int defaultValue, int& settingReference, std::pair<int, int> range, CString toolTipText) {
        ASSERT(range.first <= defaultValue && defaultValue <= range.second); // Default value not in range?
        if (range.first > settingReference || settingReference > range.second) {
            // In case settings were corrupted, reset to default.
            ASSERT(FALSE);
            settingReference = defaultValue;
        }
        auto pItem = std::make_shared<SettingsInt>(name, defaultValue, settingReference, range, toolTipText);
        auto eSetting = static_cast<ADVANCED_SETTINGS>(nItem);
        int iItem = m_list.InsertItem(nItem, pItem->GetName());
        m_hiddenOptions[eSetting] = pItem;

        CString str;
        str.Format(_T("%d"), pItem->GetValue());
        m_list.setItemTextWithDefaultFlag(iItem, COL_VALUE, str, !IsDefault(eSetting));
        m_list.SetItemData(iItem, nItem);
    };

    // The list parameter defines list of the strings that will be in the combobox.
    auto addComboItem = [this](int nItem, CString name, int defaultValue, int& settingReference, std::deque<CString> list, CString toolTipText) {
        auto pItem = std::make_shared<SettingsCombo>(name, defaultValue, settingReference, list, toolTipText);
        auto eSetting = static_cast<ADVANCED_SETTINGS>(nItem);
        int iItem = m_list.InsertItem(nItem, pItem->GetName());
        m_hiddenOptions[eSetting] = pItem;
        m_list.setItemTextWithDefaultFlag(iItem, COL_VALUE, list.at(settingReference), !IsDefault(eSetting));
        m_list.SetItemData(iItem, nItem);
    };

    auto addCStringItem = [this](int nItem, CString name, CString defaultValue, CString & settingReference, CString toolTipText) {
        auto pItem = std::make_shared<SettingsCString>(name, defaultValue, settingReference, toolTipText);
        auto eSetting = static_cast<ADVANCED_SETTINGS>(nItem);
        int iItem = m_list.InsertItem(nItem, pItem->GetName());
        m_hiddenOptions[eSetting] = pItem;
        m_list.setItemTextWithDefaultFlag(iItem, COL_VALUE, pItem->GetValue(), !IsDefault(eSetting));
        m_list.SetItemData(iItem, nItem);
    };

    addBoolItem(HIDE_WINDOWED, IDS_RS_HIDE_WINDOWED_CONTROLS, false, s.bHideWindowedControls, StrRes(IDS_PPAGEADVANCED_HIDE_WINDOWED));
    addBoolItem(MPCTHEME_MODERNSEEKBAR, IDS_RS_MODERNSEEKBAR, true, s.bModernSeekbar, StrRes(IDS_PPAGEADVANCED_MODERNSEEKBAR));
    addIntItem(MODERNSEEKBAR_HEIGHT, IDS_RS_MODERNSEEKBARHEIGHT, DEF_MODERN_SEEKBAR_HEIGHT, s.iModernSeekbarHeight, std::make_pair(MIN_MODERN_SEEKBAR_HEIGHT, MAX_MODERN_SEEKBAR_HEIGHT), StrRes(IDS_PPAGEADVANCED_MODERNSEEKBARHEIGHT));
    addIntItem(DEFAULT_TOOLBAR_SIZE, IDS_RS_DEFAULTTOOLBARSIZE, 24, s.nDefaultToolbarSize,
        std::make_pair(16, 128), StrRes(IDS_PPAGEADVANCED_DEFAULTTOOLBARSIZE));
    addBoolItem(USE_LEGACY_TOOLBAR, IDS_RS_USE_LEGACY_TOOLBAR, false, s.bUseLegacyToolbar, StrRes(IDS_PPAGEADVANCED_USE_LEGACY_TOOLBAR));
    addBoolItem(VIDEOINFO_STATUSBAR, IDS_RS_SHOW_VIDEOINFO_STATUSBAR, false, s.bShowVideoInfoInStatusbar, StrRes(IDS_PPAGEADVANCED_SHOW_VIDEOINFO_STATUSBAR));
    addBoolItem(LANG_STATUSBAR, IDS_RS_SHOW_LANG_STATUSBAR, false, s.bShowLangInStatusbar, StrRes(IDS_PPAGEADVANCED_SHOW_LANG_STATUSBAR));
    addBoolItem(FPS_STATUSBAR, IDS_RS_SHOW_FPS_STATUSBAR, false, s.bShowFPSInStatusbar, StrRes(IDS_PPAGEADVANCED_SHOW_FPS_STATUSBAR));
    addBoolItem(ABMARKS_STATUSBAR, IDS_RS_SHOW_ABMARKS_STATUSBAR, false, s.bShowABMarksInStatusbar, StrRes(IDS_PPAGEADVANCED_SHOW_ABMARKS_STATUSBAR));
    addIntItem(RECENT_FILES_NB, IDS_RS_RECENT_FILES_NUMBER, 100, s.iRecentFilesNumber, std::make_pair(0, 1000), StrRes(IDS_PPAGEADVANCED_RECENT_FILES_NUMBER));
    addIntItem(FILE_POS_LONGER, IDS_RS_FILEPOSLONGER, 0, s.iRememberPosForLongerThan, std::make_pair(0, INT_MAX), StrRes(IDS_PPAGEADVANCED_FILE_POS_LONGER));
    addBoolItem(FILE_POS_AUDIO, IDS_RS_FILEPOSAUDIO, true, s.bRememberPosForAudioFiles, StrRes(IDS_PPAGEADVANCED_FILE_POS_AUDIO));
    addBoolItem(FULLSCREEN_SEPARATE_CONTROLS, IDS_RS_FULLSCREEN_SEPARATE_CONTROLS, false, s.bFullscreenSeparateControls, _T("Keep main window controls visible on current monitor when going fullscreen on another monitor."));
    addIntItem(COVER_SIZE_LIMIT, IDS_RS_COVER_ART_SIZE_LIMIT, 600, s.nCoverArtSizeLimit, std::make_pair(0, INT_MAX), StrRes(IDS_PPAGEADVANCED_COVER_SIZE_LIMIT));
    addBoolItem(BLOCK_VSFILTER, IDS_RS_BLOCKVSFILTER, true, s.fBlockVSFilter, StrRes(IDS_PPAGEADVANCED_BLOCK_VSFILTER));
    addBoolItem(BLOCK_RDP, IDS_RS_BLOCKRDP, false, s.bBlockRDP, _T("Block use of RDP Redirection Filter"));
    addBoolItem(LOOP_FOLDER_NEXT_FILE, IDS_RS_LOOP_FOLDER_NEXT_FILE, false, s.bLoopFolderOnPlayNextFile, StrRes(IDS_PPAGEADVANCED_LOOP_FOLDER_NEXT_FILE));
    addBoolItem(USE_YDL, IDS_RS_USE_YDL, true, s.bUseYDL, StrRes(IDS_PPAGEADVANCED_USE_YDL));
    addIntItem(YDL_MAX_HEIGHT, IDS_RS_YDL_MAX_HEIGHT, 1440, s.iYDLMaxHeight, std::make_pair(0, INT_MAX), StrRes(IDS_PPAGEADVANCED_YDL_MAX_HEIGHT));
    addIntItem(YDL_VIDEO_FORMAT, IDS_RS_YDL_VIDEO_FORMAT, 0, s.iYDLVideoFormat, std::make_pair(0, 8), StrRes(IDS_PPAGEADVANCED_YDL_VIDEO_FORMAT));
    addBoolItem(YDL_AUDIO_ONLY, IDS_RS_YDL_AUDIO_ONLY, false, s.bYDLAudioOnly, StrRes(IDS_PPAGEADVANCED_YDL_AUDIO_ONLY));
    addCStringItem(YDL_EXEPATH, IDS_RS_YDL_EXEPATH, _T(""), s.sYDLExePath, _T("Full path of the Youtube-DL executable.\nJust the filename is enough in case it is located in same folder as MPC-HC.\nLeave empty to use default filename: youtube-dl.exe"));
    addCStringItem(YDL_COMMAND_LINE, IDS_RS_YDL_COMMAND_LINE, _T(""), s.sYDLCommandLine, StrRes(IDS_PPAGEADVANCED_YDL_COMMAND_LINE));
    addCStringItem(YDL_SUBS_PREFERENCE, IDS_RS_YDL_SUBS_PREFERENCE, _T(""), s.sYDLSubsPreference, _T("The language preference for loading subtitle streams extracted by Youtube-DL.\nIf empty, no subtitles are loaded.\nUse ISO639-1 two letter language code(s).\nExample: en de es pl zh ja nl"));
    addBoolItem(USE_AUTOMATIC_CAPTIONS, IDS_RS_USE_AUTOMATIC_CAPTIONS, false, s.bUseAutomaticCaptions, _T("Use automatic generated captions from Youtube. Note: this can cause a small delay when loading the stream."));
    addBoolItem(SAVEIMAGE_POSITION, IDS_RS_SAVEIMAGE_POSITION, true, s.bSaveImagePosition, StrRes(IDS_PPAGEADVANCED_SAVEIMAGE_POSITION));
    addBoolItem(SAVEIMAGE_CURRENTTIME, IDS_RS_SAVEIMAGE_CURRENTTIME, false, s.bSaveImageCurrentTime, StrRes(IDS_PPAGEADVANCED_SAVEIMAGE_CURRENTTIME));
    addBoolItem(SNAPSHOTSUBTITLES, IDS_RS_SNAPSHOTSUBTITLES, true, s.bSnapShotSubtitles, StrRes(IDS_PPAGEADVANCED_SNAPSHOTSUBTITLES));
    addBoolItem(SNAPSHOTKEEPVIDEOEXTENSION, IDS_RS_SNAPSHOTKEEPVIDEOEXTENSION, true, s.bSnapShotKeepVideoExtension, StrRes(IDS_PPAGEADVANCED_SNAPSHOTKEEPVIDEOEXTENSION));
    addBoolItem(USE_SMTC, IDS_RS_USE_SMTC, false, s.bUseSMTC, _T("Allow control of the player through System Media Transport Controls in Windows 8.1/10/11."));
    addBoolItem(ADD_LANGCODE_WHEN_SAVE_SUBTITLES, IDS_RS_ADD_LANGCODE_WHEN_SAVE_SUBTITLES, false, s.bAddLangCodeWhenSaveSubtitles, StrRes(IDS_PPAGEADVANCED_ADD_LANGCODE_WHEN_SAVE_SUBTITLES));
    addBoolItem(USE_TITLE_IN_RECENT_FILE_LIST, IDS_RS_USE_TITLE_IN_RECENT_FILE_LIST, true, s.bUseTitleInRecentFileList, StrRes(IDS_PPAGEADVANCED_USE_TITLE_IN_RECENT_FILE_LIST));
    addBoolItem(LOCK_NOPAUSE, IDS_RS_LOCK_NOPAUSE, false, s.bLockNoPause, _T("Do not pause playback when locking the screen."));
    addIntItem(RELOAD_AFTER_LONG_PAUSE, IDS_RS_RELOAD_AFTER_LONG_PAUSE, -1, s.iReloadAfterLongPause, std::make_pair(-1, 1440), _T("Reload video file before resuming playback if it was paused for more than X minutes. Use 0 to disable automatic file reload after pause. Use -1 to also disable reload after hibernate/sleep."));
    addBoolItem(INACCURATE_FASTSEEK, IDS_RS_ALLOW_INACCURATE_FASTSEEK, true, s.bAllowInaccurateFastseek, StrRes(IDS_PPAGEADVANCED_ALLOW_INACCURATE_FASTSEEK));
    addIntItem(STREAMPOSPOLLER_INTERVAL, IDS_RS_TIME_REFRESH_INTERVAL, 100, s.nStreamPosPollerInterval, std::make_pair(40, 500), StrRes(IDS_PPAGEADVANCED_TIME_REFRESH_INTERVAL));
    addIntItem(REDIR_OPEN_TO_APPEND, IDS_RS_REDIRECT_OPEN_TO_APPEND_THRESHOLD, 1000, s.iRedirectOpenToAppendThreshold, std::make_pair(250, 5000), _T("If a file is opened within this amount of milliseconds after a previous file, then it will be appended to playlist instead of replacing that previous file. This redirection is required for properly handling opening of a selection of files in Explorer. A too short threshold can result in only a part of your selection being added to playlist."));
#if !defined(_DEBUG) && USE_DRDUMP_CRASH_REPORTER
    addBoolItem(CRASHREPORTER, IDS_RS_ENABLE_CRASH_REPORTER, true, s.bEnableCrashReporter, StrRes(IDS_PPAGEADVANCED_CRASHREPORTER));
#endif
    addBoolItem(LOGGING, IDS_RS_LOGGING, false, s.bEnableLogging, StrRes(IDS_PPAGEADVANCED_LOGGER));
    addIntItem(FULLSCREEN_DELAY, IDS_RS_FULLSCREEN_DELAY, MIN_FULLSCREEN_DELAY, s.iFullscreenDelay, std::make_pair(MIN_FULLSCREEN_DELAY, MAX_FULLSCREEN_DELAY), StrRes(IDS_PPAGEADVANCED_FULLSCREEN_DELAY));
    addIntItem(AUTO_DOWNLOAD_SCORE_MOVIES, IDS_RS_AUTODOWNLOADSCOREMOVIES, 0x16, s.nAutoDownloadScoreMovies,
        std::make_pair(10, 30), StrRes(IDS_PPAGEADVANCED_SCORE));
    addIntItem(AUTO_DOWNLOAD_SCORE_SERIES, IDS_RS_AUTODOWNLOADSCORESERIES, 0x18, s.nAutoDownloadScoreSeries,
        std::make_pair(10, 30), StrRes(IDS_PPAGEADVANCED_SCORE));
    addBoolItem(OPEN_REC_PANEL_WHEN_OPENING_DEVICE, IDS_RS_OPEN_REC_PANEL_WHEN_OPENING_DEVICE, true, s.bOpenRecPanelWhenOpeningDevice, StrRes(IDS_PPAGEADVANCED_OPEN_REC_PANEL_WHEN_OPENING_DEVICE));
}

BOOL CPPageAdvanced::OnApply()
{
    auto& s = AfxGetAppSettings();

    int nOldDefaultToolbarSize = s.nDefaultToolbarSize;

    for (int i = 0; i < m_list.GetItemCount(); i++) {
        auto eSetting = static_cast<ADVANCED_SETTINGS>(m_list.GetItemData(i));
        m_hiddenOptions.at(eSetting)->Apply();
    }

    s.MRU.SetSize(s.iRecentFilesNumber);

#if !defined(_DEBUG) && USE_DRDUMP_CRASH_REPORTER
    if (!s.bEnableCrashReporter && CrashReporter::IsEnabled()) {
        CrashReporter::Disable();
        MPCExceptionHandler::Enable();
    }
#endif

    // There is no main frame when the option dialog is displayed stand-alone
    if (CMainFrame* pMainFrame = AfxGetMainFrame()) {
        pMainFrame->UpdateControlState(CMainFrame::UPDATE_CONTROLS_VISIBILITY);
        pMainFrame->AdjustStreamPosPoller(true);
    }

    if (nOldDefaultToolbarSize != s.nDefaultToolbarSize) {
        m_eventc.FireEvent(MpcEvent::DEFAULT_TOOLBAR_SIZE_CHANGED);
        if (CMainFrame* pMainFrame = AfxGetMainFrame()) {
            pMainFrame->RecalcLayout();
        }
    }

    return __super::OnApply();
}

bool CPPageAdvanced::IsDefault(ADVANCED_SETTINGS eSetting) const
{
    return m_hiddenOptions.at(eSetting)->IsDefault();
}

BEGIN_MESSAGE_MAP(CPPageAdvanced, CMPCThemePPageBase)
    ON_BN_CLICKED(IDC_BUTTON1, OnBnClickedDefaultButton)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON1, OnUpdateDefaultButton)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST1, OnNMDblclk)
    ON_NOTIFY(NM_CUSTOMDRAW, IDC_LIST1, OnNMCustomdraw)
    ON_NOTIFY(LVN_GETINFOTIP, IDC_LIST1, OnLvnGetInfoTipList)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST1, OnLvnItemchangedList)
    ON_BN_CLICKED(IDC_RADIO1, OnBnClickedRadio1)
    ON_BN_CLICKED(IDC_RADIO2, OnBnClickedRadio2)
    ON_CBN_SELCHANGE(IDC_COMBO1, OnCbnSelchangeCombobox)
    ON_EN_CHANGE(IDC_EDIT1, OnEnChangeEdit)
END_MESSAGE_MAP()

void CPPageAdvanced::OnBnClickedDefaultButton()
{
    UpdateData();

    const int iItem = GetListSelectionMark();
    if (iItem >= 0) {
        CString str;
        auto eSetting = static_cast<ADVANCED_SETTINGS>(m_list.GetItemData(iItem));
        auto pItem = m_hiddenOptions.at(eSetting);
        pItem->ResetDefault();

        if (auto pItemBool = std::dynamic_pointer_cast<SettingsBool>(pItem)) {
            str = pItemBool->GetValue() ? m_strTrue : m_strFalse;
            SetDlgItemText(IDC_EDIT1, str);
            if (pItemBool->GetValue()) {
                CheckRadioButton(IDC_RADIO1, IDC_RADIO2, IDC_RADIO1);
            } else {
                CheckRadioButton(IDC_RADIO1, IDC_RADIO2, IDC_RADIO2);
            }
        } else if (auto pItemCombo = std::dynamic_pointer_cast<SettingsCombo>(pItem)) {
            const auto& list = pItemCombo->GetList();
            str = list.at(pItemCombo->GetValue());
            m_comboBox.SetCurSel(pItemCombo->GetValue());
        } else if (auto pItemInt = std::dynamic_pointer_cast<SettingsInt>(pItem)) {
            SetDlgItemInt(IDC_EDIT1, pItemInt->GetValue());
            str.Format(_T("%d"), pItemInt->GetValue());
        } else if (auto pItemCString = std::dynamic_pointer_cast<SettingsCString>(pItem)) {
            str = pItemCString->GetValue();
            SetDlgItemText(IDC_EDIT1, pItemCString->GetValue());
        } else {
            UNREACHABLE_CODE();
        }

        m_list.setItemTextWithDefaultFlag(iItem, COL_VALUE, str, !IsDefault(eSetting));
        UpdateData(FALSE);
        m_list.Update(iItem);
        SetModified();
    }
}

void CPPageAdvanced::OnUpdateDefaultButton(CCmdUI* pCmdUI)
{
    const int iItem = GetListSelectionMark();
    bool bEnable = false;
    if (iItem >= 0) {
        auto eSetting = static_cast<ADVANCED_SETTINGS>(m_list.GetItemData(iItem));
        bEnable = !IsDefault(eSetting);
    }
    pCmdUI->Enable(bEnable);
}

void CPPageAdvanced::OnNMDblclk(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
    if (pNMItemActivate->iItem >= 0) {
        auto eSetting = static_cast<ADVANCED_SETTINGS>(m_list.GetItemData(pNMItemActivate->iItem));
        auto pItem = m_hiddenOptions.at(eSetting);
        if (auto pItemBool = std::dynamic_pointer_cast<SettingsBool>(pItem)) {
            pItemBool->Toggle();
            m_list.setItemTextWithDefaultFlag(pNMItemActivate->iItem, COL_VALUE,
                                              pItemBool->GetValue() ? m_strTrue : m_strFalse, !IsDefault(eSetting));
            if (pItemBool->GetValue()) {
                CheckRadioButton(IDC_RADIO1, IDC_RADIO2, IDC_RADIO1);
            } else {
                CheckRadioButton(IDC_RADIO1, IDC_RADIO2, IDC_RADIO2);
            }
            UpdateData(FALSE);
            m_list.Update(pNMItemActivate->iItem);
            SetModified();
        }
    }
    *pResult = 0;
}

void CPPageAdvanced::OnNMCustomdraw(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMCUSTOMDRAW pNMCD = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);

    switch (pNMCD->dwDrawStage) {
        case CDDS_PREPAINT:
            *pResult = CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
            break;
        case CDDS_ITEMPREPAINT: {
            auto eSetting = static_cast<ADVANCED_SETTINGS>(m_list.GetItemData((int)pNMCD->dwItemSpec));
            if (!IsDefault(eSetting)) {
                ::SelectObject(pNMCD->hdc, m_fontBold.GetSafeHandle());
                *pResult |= CDRF_NEWFONT;
            } else {
                *pResult = CDRF_DODEFAULT;
            }
        }
        break;
        default:
            *pResult = CDRF_DODEFAULT;
    }
}

void CPPageAdvanced::OnLvnGetInfoTipList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLVGETINFOTIP pGetInfoTip = reinterpret_cast<LPNMLVGETINFOTIP>(pNMHDR);

    auto eSetting = static_cast<ADVANCED_SETTINGS>(m_list.GetItemData(pGetInfoTip->iItem));
    auto pItem = m_hiddenOptions.at(eSetting);
    StringCchCopy(pGetInfoTip->pszText, pGetInfoTip->cchTextMax, pItem->GetToolTipText());

    *pResult = 0;
}

void CPPageAdvanced::OnLvnItemchangedList(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

    if ((pNMLV->uChanged & LVIF_STATE) && (pNMLV->uNewState & LVNI_SELECTED)) {
        auto setDialogItemsVisibility = [this](std::initializer_list<int>&& ids, int nCmdShow) {
            for (const auto& nID : ids) {
                GetDlgItem(nID)->ShowWindow(nCmdShow);
            }
        };

        if (pNMLV->iItem >= 0) {
            m_lastSelectedItem = pNMLV->iItem;
            auto eSetting = static_cast<ADVANCED_SETTINGS>(m_list.GetItemData(pNMLV->iItem));
            auto pItem = m_hiddenOptions.at(eSetting);
            GetDlgItem(IDC_BUTTON1)->ShowWindow(SW_SHOW);

            if (auto pItemBool = std::dynamic_pointer_cast<SettingsBool>(pItem)) {
                setDialogItemsVisibility({ IDC_EDIT1, IDC_COMBO1, IDC_SPIN1 }, SW_HIDE);
                if (pItemBool->GetValue()) {
                    CheckRadioButton(IDC_RADIO1, IDC_RADIO2, IDC_RADIO1);
                } else {
                    CheckRadioButton(IDC_RADIO1, IDC_RADIO2, IDC_RADIO2);
                }
                setDialogItemsVisibility({ IDC_RADIO1, IDC_RADIO2 }, SW_SHOW);
            } else if (auto pItemCombo = std::dynamic_pointer_cast<SettingsCombo>(pItem)) {
                setDialogItemsVisibility({ IDC_EDIT1, IDC_RADIO1, IDC_RADIO2, IDC_SPIN1 }, SW_HIDE);
                m_comboBox.ResetContent();
                for (const auto& str : pItemCombo->GetList()) {
                    m_comboBox.AddString(str);
                }
                m_comboBox.SetCurSel(pItemCombo->GetValue());
                m_comboBox.ShowWindow(SW_SHOW);
            } else if (auto pItemInt = std::dynamic_pointer_cast<SettingsInt>(pItem)) {
                setDialogItemsVisibility({ IDC_COMBO1, IDC_RADIO1, IDC_RADIO2 }, SW_HIDE);
                GetDlgItem(IDC_EDIT1)->ModifyStyle(0, ES_NUMBER, 0);
                const auto& range = pItemInt->GetRange();
                if (!m_spinButtonCtrl.GetBuddy()) {
                    GetDlgItem(IDC_EDIT1)->MoveWindow(editRect, TRUE);
                    m_spinButtonCtrl.SetBuddy(GetDlgItem(IDC_EDIT1));
                }
                m_spinButtonCtrl.SetRange32(range.first, range.second);
                m_spinButtonCtrl.SetPos32(pItemInt->GetValue());
                m_spinButtonCtrl.ShowWindow(SW_SHOW);
                GetDlgItem(IDC_EDIT1)->ShowWindow(SW_SHOW);
            } else if (auto pItemCString = std::dynamic_pointer_cast<SettingsCString>(pItem)) {
                setDialogItemsVisibility({ IDC_COMBO1, IDC_RADIO1, IDC_RADIO2, IDC_BUTTON1, IDC_SPIN1 }, SW_HIDE);
                GetDlgItem(IDC_EDIT1)->ModifyStyle(ES_NUMBER, 0, 0);
                SetDlgItemText(IDC_EDIT1, pItemCString->GetValue());
                m_spinButtonCtrl.SetBuddy(NULL);
                GetDlgItem(IDC_EDIT1)->ShowWindow(SW_SHOW);
            } else {
                UNREACHABLE_CODE();
            }
        } else {
            setDialogItemsVisibility({ IDC_EDIT1, IDC_COMBO1, IDC_RADIO1, IDC_RADIO2, IDC_BUTTON1, IDC_SPIN1 }, SW_HIDE);
        }
    }

    *pResult = 0;
}

void CPPageAdvanced::OnBnClickedRadio1()
{
    const int iItem = GetListSelectionMark();
    if (iItem >= 0) {
        auto eSetting = static_cast<ADVANCED_SETTINGS>(m_list.GetItemData(iItem));
        auto pItem = m_hiddenOptions.at(eSetting);

        if (auto pItemBool = std::dynamic_pointer_cast<SettingsBool>(pItem)) {
            pItemBool->SetValue(true);
            m_list.setItemTextWithDefaultFlag(iItem, COL_VALUE, m_strTrue, !IsDefault(eSetting));
            UpdateData(FALSE);
            m_list.Update(iItem);
            SetModified();
        }
    }
}

void CPPageAdvanced::OnBnClickedRadio2()
{
    const int iItem = GetListSelectionMark();
    if (iItem >= 0) {
        auto eSetting = static_cast<ADVANCED_SETTINGS>(m_list.GetItemData(iItem));
        auto pItem = m_hiddenOptions.at(eSetting);

        if (auto pItemBool = std::dynamic_pointer_cast<SettingsBool>(pItem)) {
            pItemBool->SetValue(false);
            m_list.setItemTextWithDefaultFlag(iItem, COL_VALUE, m_strFalse, !IsDefault(eSetting));
            UpdateData(FALSE);
            m_list.Update(iItem);
            SetModified();
        }
    }
}

void CPPageAdvanced::OnCbnSelchangeCombobox()
{
    int iItem = m_comboBox.GetCurSel();
    const int nItem = GetListSelectionMark();
    if (iItem >= 0) {
        auto eSetting = static_cast<ADVANCED_SETTINGS>(m_list.GetItemData(nItem));
        auto pItem = m_hiddenOptions.at(eSetting);

        if (auto pItemCombo = std::dynamic_pointer_cast<SettingsCombo>(pItem)) {
            auto list = pItemCombo->GetList();
            pItemCombo->SetValue(iItem);
            m_list.setItemTextWithDefaultFlag(iItem, COL_VALUE, list.at(iItem), !IsDefault(eSetting));
            UpdateData(FALSE);
            m_list.Update(nItem);
            if (m_comboBox.IsWindowVisible()) {
                SetModified();
            }
        }
    }
}

void CPPageAdvanced::OnEnChangeEdit()
{
    UpdateData();

    const int iItem = GetListSelectionMark();
    if (iItem >= 0) {
        CString str;
        auto eSetting = static_cast<ADVANCED_SETTINGS>(m_list.GetItemData(iItem));
        auto pItem = m_hiddenOptions.at(eSetting);
        bool bChanged = false;

        if (auto pItemCombo = std::dynamic_pointer_cast<SettingsCombo>(pItem)) {
            ASSERT(FALSE);
        } else if (auto pItemInt = std::dynamic_pointer_cast<SettingsInt>(pItem)) {
            BOOL lpTrans;
            const auto& range = pItemInt->GetRange();
            int newValue = GetDlgItemInt(IDC_EDIT1, &lpTrans);
            if (!!lpTrans && range.first <= newValue && newValue <= range.second) {
                bChanged = newValue != pItemInt->GetValue();
                if (bChanged) {
                    pItemInt->SetValue(newValue);
                    str.Format(_T("%d"), pItemInt->GetValue());
                }
            } else {
                SetDlgItemInt(IDC_EDIT1, pItemInt->GetValue());
            }
        } else if (auto pItemCString = std::dynamic_pointer_cast<SettingsCString>(pItem)) {
            GetDlgItemText(IDC_EDIT1, str);
            bChanged = str != pItemCString->GetValue();
            if (bChanged) {
                pItemCString->SetValue(str);
            }
        }

        if (bChanged) {
            m_list.setItemTextWithDefaultFlag(iItem, COL_VALUE, str, !IsDefault(eSetting));
            m_list.Update(iItem);
            UpdateData(FALSE);
            SetModified();
        }
    }
}
