/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2016 see Authors.txt
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
#include "mplayerc.h"
#include <cmath>
#include <atlbase.h>
#include <afxpriv.h>
#include "MPCPngImage.h"
#include "PlayerToolBar.h"
#include "MainFrm.h"
#include "PathUtils.h"
#include "SVGImage.h"
#include "ImageGrayer.h"
#include "CMPCTheme.h"

// CPlayerToolBar

IMPLEMENT_DYNAMIC(CPlayerToolBar, CToolBar)
CPlayerToolBar::CPlayerToolBar(CMainFrame* pMainFrame)
    : m_pMainFrame(pMainFrame)
    , m_nButtonHeight(16)
    , m_volumeCtrlSize(60)
    , mouseDownL(false)
    , mouseDownR(false)
{
    GetEventd().Connect(m_eventc, {
        MpcEvent::DPI_CHANGED,
        MpcEvent::DEFAULT_TOOLBAR_SIZE_CHANGED,
    }, std::bind(&CPlayerToolBar::EventCallback, this, std::placeholders::_1));
}

CPlayerToolBar::~CPlayerToolBar()
{
}

bool CPlayerToolBar::LoadExternalToolBar(CImage& image, bool useColor)
{
    // Paths and extensions to try (by order of preference)
    std::vector<CString> paths({ PathUtils::GetProgramPath() });
    CString appDataPath;
    if (AfxGetMyApp()->GetAppDataPath(appDataPath)) {
        paths.emplace_back(appDataPath);
    }
    const std::vector<CString> extensions({ _T("png"), _T("bmp") });
    CString basetbname;
    if (AppIsThemeLoaded()) {
        const auto& s = AfxGetAppSettings();
        if (s.eModernThemeMode == CMPCTheme::ModernThemeMode::DARK || s.eModernThemeMode == CMPCTheme::ModernThemeMode::WINDOWSDEFAULT && s.bWindows10DarkThemeActive) {
            basetbname = _T("toolbar_dark.");
        } else {
            basetbname = _T("toolbar_light.");
        }
    } else {
        basetbname = _T("toolbar.");
    }

    if (useColor) {
        basetbname = _T("color_") + basetbname;
    }

    // TODO: Find a better solution?
    float dpiScaling = (float)std::min(m_pMainFrame->m_dpi.ScaleFactorX(), m_pMainFrame->m_dpi.ScaleFactorY());

    // Try loading the external toolbar
    for (const auto& path : paths) {
        if (SUCCEEDED(SVGImage::Load(PathUtils::CombinePaths(path, basetbname + _T("svg")), image, dpiScaling))) {
            return true;
        }

        for (const auto& ext : extensions) {
            if (SUCCEEDED(image.Load(PathUtils::CombinePaths(path, basetbname + ext)))) {
                return true;
            }
        }
    }

    return false;
}

void CPlayerToolBar::LoadToolbarImage()
{
    // We are currently not aware of any cases where the scale factors are different
    float dpiScaling = (float)std::min(m_pMainFrame->m_dpi.ScaleFactorX(), m_pMainFrame->m_dpi.ScaleFactorY());
    int targetsize = int(dpiScaling * AfxGetAppSettings().nDefaultToolbarSize);
    float svgscale = targetsize / 16.0f;

    CImage image, themedImage, origImage;
    m_pButtonsImages.reset();
    m_pDisabledButtonsImages.reset();

    bool colorToolbar = false, toolbarImageLoaded = false;
    if (LoadExternalToolBar(origImage, true)) {
        colorToolbar = true;
        toolbarImageLoaded = true;
    } else if (LoadExternalToolBar(origImage, false)) {
        toolbarImageLoaded = true;
    }

    if (toolbarImageLoaded || (!AfxGetAppSettings().bUseLegacyToolbar && SUCCEEDED(SVGImage::Load(IDF_SVG_TOOLBAR, origImage, svgscale)))) {
        if (AppIsThemeLoaded() && colorToolbar == false) {
            ImageGrayer::UpdateColor(origImage, themedImage, false, ImageGrayer::mpcMono);
            image = themedImage;
        } else {
            image = origImage;
        }
        CBitmap* bmp = CBitmap::FromHandle(image);
        int width = image.GetWidth();
        int height = image.GetHeight();
        int bpp = image.GetBPP();
        if (width == height * 15) {
            // the manual specifies that sizeButton should be sizeImage inflated by (7, 6)
            SetSizes(CSize(height + 7, height + 6), CSize(height, height));

            m_pButtonsImages.reset(DEBUG_NEW CImageList());
            if (bpp == 32) {
                m_pButtonsImages->Create(height, height, ILC_COLOR32 | ILC_MASK, 1, 0);
                m_pButtonsImages->Add(bmp, nullptr); // alpha is the mask

                if (colorToolbar == false) {//if color toolbar, we assume the imagelist can grey itself nicely, rather than using imagegrayer
                    CImage imageDisabled;
                    if (ImageGrayer::UpdateColor(origImage, imageDisabled, true, AppIsThemeLoaded() ? ImageGrayer::mpcMono : ImageGrayer::classicGrayscale)) {
                        m_pDisabledButtonsImages.reset(DEBUG_NEW CImageList());
                        m_pDisabledButtonsImages->Create(height, height, ILC_COLOR32 | ILC_MASK, 1, 0);
                        m_pDisabledButtonsImages->Add(CBitmap::FromHandle(imageDisabled), nullptr); // alpha is the mask
                        imageDisabled.Destroy();
                    } else {
                        m_pDisabledButtonsImages = nullptr;
                    }
                } else {
                    m_pDisabledButtonsImages = nullptr;
                }
            } else {
                m_pButtonsImages->Create(height, height, ILC_COLOR24 | ILC_MASK, 1, 0);
                m_pButtonsImages->Add(bmp, RGB(255, 0, 255));
            }
            m_nButtonHeight = height;
            GetToolBarCtrl().SetImageList(m_pButtonsImages.get());
            GetToolBarCtrl().SetDisabledImageList(m_pDisabledButtonsImages.get());
        }
        if (themedImage) {
            themedImage.Destroy();
        }
    }
    origImage.Destroy();
}

BOOL CPlayerToolBar::Create(CWnd* pParentWnd)
{
    VERIFY(__super::CreateEx(pParentWnd,
                             TBSTYLE_FLAT | TBSTYLE_TRANSPARENT | TBSTYLE_AUTOSIZE | TBSTYLE_CUSTOMERASE,
                             WS_CHILD | WS_VISIBLE | CBRS_BOTTOM /*| CBRS_TOOLTIPS*/,
                             CRect(2, 2, 0, 1)));

    VERIFY(LoadToolBar(IDB_PLAYERTOOLBAR));

    // Should never be RTLed
    ModifyStyleEx(WS_EX_LAYOUTRTL, WS_EX_NOINHERITLAYOUT);

    CToolBarCtrl& tb = GetToolBarCtrl();
    tb.DeleteButton(tb.GetButtonCount() - 1);
    tb.DeleteButton(tb.GetButtonCount() - 1);

    SetMute(AfxGetAppSettings().fMute);

    UINT styles[] = {
        TBBS_CHECKGROUP, TBBS_CHECKGROUP, TBBS_CHECKGROUP,
        TBBS_SEPARATOR,
        TBBS_BUTTON, TBBS_BUTTON, TBBS_BUTTON, TBBS_BUTTON,
        TBBS_SEPARATOR,
        TBBS_BUTTON,
        TBBS_SEPARATOR,
        TBBS_SEPARATOR, // variable spacing between the regular controls and mute button
        TBBS_CHECKBOX,
    };

    for (int i = 0; i < _countof(styles); ++i) {
        // This fixes missing separator in Win 7
        if (styles[i] & TBBS_SEPARATOR) {
            SetButtonInfo(i, GetItemID(i), styles[i], -1);
        } else {
            SetButtonStyle(i, styles[i] | TBBS_DISABLED);
        }
    }

    m_volctrl.Create(this);
    m_volctrl.SetRange(0, 100);

    m_nButtonHeight = 16; // reset m_nButtonHeight

    LoadToolbarImage();

    if (AppIsThemeLoaded()) {
        themedToolTip.enableFlickerHelper(); //avoid flicker on button hover
        themedToolTip.Create(this, TTS_ALWAYSTIP);
        tb.SetToolTips(&themedToolTip);
    } else {
        tb.EnableToolTips();
    }

    return TRUE;
}

void CPlayerToolBar::ArrangeControls() {
    if (!::IsWindow(m_volctrl.m_hWnd)) {
        return;
    }

    CRect r;
    GetClientRect(&r);

    CRect br = GetBorders();

    CRect vr;
    if (AppIsThemeLoaded()) {
        float dpiScaling = (float)std::min(m_pMainFrame->m_dpi.ScaleFactorX(), m_pMainFrame->m_dpi.ScaleFactorY());
        int targetsize = int(dpiScaling * AfxGetAppSettings().nDefaultToolbarSize);

        m_volumeCtrlSize = targetsize * 2.5f;
        vr = CRect(r.right + br.right - m_volumeCtrlSize, r.top+targetsize/4, r.right + br.right, r.bottom-targetsize/4);
    } else {
        vr = CRect(r.right + br.right - 58, r.top - 2, r.right + br.right + 6, r.bottom);
        m_volctrl.MoveWindow(vr);

        CRect thumbRect;
        m_volctrl.GetThumbRect(thumbRect);
        m_volctrl.MapWindowPoints(this, thumbRect);
        vr.top += std::max((r.bottom - thumbRect.bottom - 4) / 2, 0l);
        vr.left -= MulDiv(thumbRect.Height(), 50, 19) - 50;
        m_volumeCtrlSize = vr.Width();
    }
    m_volctrl.MoveWindow(vr);

    CRect r10; // last normal separator
    GetItemRect(10, &r10);
    CRect r12; // mute button
    GetItemRect(12, &r12);

    // adjust spacing between controls and mute
    int spacing = vr.left - r10.right - r12.Width();
    SetButtonInfo(11, GetItemID(11), TBBS_SEPARATOR, spacing);
}

void CPlayerToolBar::SetMute(bool fMute)
{
    CToolBarCtrl& tb = GetToolBarCtrl();
    TBBUTTONINFO bi;
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_IMAGE;
    bi.iImage = fMute ? 13 : 12;
    tb.SetButtonInfo(ID_VOLUME_MUTE, &bi);

    AfxGetAppSettings().fMute = fMute;
}

bool CPlayerToolBar::IsMuted() const
{
    CToolBarCtrl& tb = GetToolBarCtrl();
    TBBUTTONINFO bi;
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_IMAGE;
    tb.GetButtonInfo(ID_VOLUME_MUTE, &bi);
    return (bi.iImage == 13);
}

int CPlayerToolBar::GetVolume() const
{
    int volume = m_volctrl.GetPos(); // [0..100]
    if (IsMuted() || volume <= 0) {
        volume = -10000;
    } else {
        volume = std::min((int)(4000 * log10(volume / 100.0f)), 0); // 4000=2.0*100*20, where 2.0 is a special factor
    }

    return volume;
}

int CPlayerToolBar::GetMinWidth() const
{
    // button widths are inflated by 7px
    // 9 buttons + 3 separators + spacing + volume
    return 9 * (m_nButtonHeight + 7) + 3 * 8 + 4 + m_volumeCtrlSize;
}

void CPlayerToolBar::SetVolume(int volume)
{
    m_volctrl.SetPosInternal(volume);
}

void CPlayerToolBar::EventCallback(MpcEvent ev)
{
    switch (ev) {
        case MpcEvent::DPI_CHANGED:
        case MpcEvent::DEFAULT_TOOLBAR_SIZE_CHANGED:
            LoadToolbarImage();
            break;
        default:
            UNREACHABLE_CODE();
    }
}

BEGIN_MESSAGE_MAP(CPlayerToolBar, CToolBar)
    ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, OnCustomDraw)
    ON_WM_SIZE()
    ON_MESSAGE_VOID(WM_INITIALUPDATE, OnInitialUpdate)
    ON_COMMAND_EX(ID_VOLUME_MUTE, OnVolumeMute)
    ON_UPDATE_COMMAND_UI(ID_VOLUME_MUTE, OnUpdateVolumeMute)
    ON_COMMAND_EX(ID_VOLUME_UP, OnVolumeUp)
    ON_COMMAND_EX(ID_VOLUME_DOWN, OnVolumeDown)
    ON_WM_NCPAINT()
    ON_WM_LBUTTONDOWN()
    ON_WM_RBUTTONDOWN()
    ON_WM_SETCURSOR()
    ON_NOTIFY_EX(TTN_NEEDTEXT, 0, OnToolTipNotify)
    ON_WM_LBUTTONUP()
    ON_WM_RBUTTONUP()
END_MESSAGE_MAP()

// CPlayerToolBar message handlers

void drawButtonBG(NMCUSTOMDRAW nmcd, COLORREF c)
{
    CDC dc;
    dc.Attach(nmcd.hdc);
    CRect br;
    br.CopyRect(&nmcd.rc);
    br.DeflateRect(0, 0, 1, 1); //we aren't offsetting button when pressed, so try to center better

    dc.FillSolidRect(br, c);

    CBrush fb;
    fb.CreateSolidBrush(CMPCTheme::PlayerButtonBorderColor);
    dc.FrameRect(br, &fb);
    fb.DeleteObject();
    dc.Detach();
}

void CPlayerToolBar::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMTBCUSTOMDRAW pTBCD = reinterpret_cast<LPNMTBCUSTOMDRAW>(pNMHDR);
    LRESULT lr = CDRF_DODEFAULT;

    switch (pTBCD->nmcd.dwDrawStage) {
        case CDDS_PREERASE:
            m_volctrl.Invalidate();
            lr = CDRF_DODEFAULT;
            break;
        case CDDS_PREPAINT: {
            // paint the control background, this is needed for XP
            CDC dc;
            dc.Attach(pTBCD->nmcd.hdc);
            RECT r;
            GetClientRect(&r);
            if (AppIsThemeLoaded()) {
                dc.FillSolidRect(&r, CMPCTheme::PlayerBGColor);
            } else {
                dc.FillSolidRect(&r, ::GetSysColor(COLOR_BTNFACE));
            }
            dc.Detach();
        }
        lr |= CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
        break;
        case CDDS_ITEMPREPAINT:
            lr |= CDRF_NOTIFYPOSTPAINT;
            {
                if (AppIsThemeLoaded()) {
                    lr |= TBCDRF_NOBACKGROUND | TBCDRF_NOOFFSET;
                    if (pTBCD->nmcd.uItemState & CDIS_CHECKED) {
                        drawButtonBG(pTBCD->nmcd, CMPCTheme::PlayerButtonCheckedColor);
                    } else if (pTBCD->nmcd.uItemState & CDIS_HOT) {
                        drawButtonBG(pTBCD->nmcd, mouseDownL ? CMPCTheme::PlayerButtonClickedColor : CMPCTheme::PlayerButtonHotColor);
                    }
                }
            }
            break;
        case CDDS_ITEMPOSTPAINT:
            // paint over the duplicated separator
            CDC dc;
            dc.Attach(pTBCD->nmcd.hdc);
            RECT r;
            GetItemRect(11, &r);
            if (AppIsThemeLoaded()) {
                dc.FillSolidRect(&r, CMPCTheme::PlayerBGColor);
            } else {
                dc.FillSolidRect(&r, GetSysColor(COLOR_BTNFACE));
            }
            dc.Detach();
            lr |= CDRF_SKIPDEFAULT;
            break;
    }

    *pResult = lr;
}

void CPlayerToolBar::OnSize(UINT nType, int cx, int cy)
{
    __super::OnSize(nType, cx, cy);

    ArrangeControls();
}

void CPlayerToolBar::OnInitialUpdate()
{
    //ArrangeControls();
}

BOOL CPlayerToolBar::OnVolumeMute(UINT nID)
{
    SetMute(!IsMuted());
    return FALSE;
}

void CPlayerToolBar::OnUpdateVolumeMute(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(true);
    pCmdUI->SetCheck(IsMuted());
}

BOOL CPlayerToolBar::OnVolumeUp(UINT nID)
{
    m_volctrl.IncreaseVolume();
    return FALSE;
}

BOOL CPlayerToolBar::OnVolumeDown(UINT nID)
{
    m_volctrl.DecreaseVolume();
    return FALSE;
}

void CPlayerToolBar::OnNcPaint() // when using XP styles the NC area isn't drawn for our toolbar...
{
    CRect wr, cr;

    CWindowDC dc(this);
    GetClientRect(&cr);
    ClientToScreen(&cr);
    GetWindowRect(&wr);
    cr.OffsetRect(-wr.left, -wr.top);
    wr.OffsetRect(-wr.left, -wr.top);
    dc.ExcludeClipRect(&cr);

    if (AppIsThemeLoaded()) {
        dc.FillSolidRect(wr, CMPCTheme::PlayerBGColor);
    } else {
        dc.FillSolidRect(wr, ::GetSysColor(COLOR_BTNFACE));
    }

    // Do not call CToolBar::OnNcPaint() for painting messages

    // Invalidate window to force repaint the expanded separator
    Invalidate(FALSE);
}

BOOL CPlayerToolBar::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    BOOL ret = FALSE;
    if (nHitTest == HTCLIENT) {
        CPoint point;
        VERIFY(GetCursorPos(&point));
        ScreenToClient(&point);

        int i = getHitButtonIdx(point);
        if (i >= 0 && !(GetButtonStyle(i) & (TBBS_SEPARATOR | TBBS_DISABLED))) {
            ::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_HAND));
            ret = TRUE;
        }
    }
    return ret ? ret : __super::OnSetCursor(pWnd, nHitTest, message);
}

void CPlayerToolBar::OnLButtonDown(UINT nFlags, CPoint point)
{
    int i = getHitButtonIdx(point);
    mouseDownL = true;

    if (!m_pMainFrame->m_fFullScreen && (i < 0 || (GetButtonStyle(i) & (TBBS_SEPARATOR | TBBS_DISABLED)))) {
        ClientToScreen(&point);
        m_pMainFrame->PostMessage(WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(point.x, point.y));
    } else {
        __super::OnLButtonDown(nFlags, point);
    }
    m_pMainFrame->RestoreFocus();
}

void CPlayerToolBar::OnRButtonDown(UINT nFlags, CPoint point) {
    int i = getHitButtonIdx(point);
    mouseDownR = true;

    if (!m_pMainFrame->m_fFullScreen && (i < 0 || (GetButtonStyle(i) & (TBBS_SEPARATOR | TBBS_DISABLED)))) {
        ClientToScreen(&point);
        m_pMainFrame->PostMessage(WM_NCRBUTTONDOWN, HTCAPTION, MAKELPARAM(point.x, point.y));
    } else {
        rightButtonIndex = i;
        __super::OnRButtonDown(nFlags, point);
    }
}

int CPlayerToolBar::getHitButtonIdx(CPoint point)
{
    int hit = -1; // -1 means not on any buttons, mute button is 12/13, others < 10, 11 is empty space between
    CRect r;

    for (int i = 0, j = GetToolBarCtrl().GetButtonCount(); i < j; i++) {
        GetItemRect(i, r);

        if (r.PtInRect(point)) {
            hit = i;
            break;
        }
    }

    return hit;
}

BOOL CPlayerToolBar::OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
    TOOLTIPTEXT* pTTT = (TOOLTIPTEXT*)pNMHDR;

    UINT_PTR nID = pNMHDR->idFrom;
    if (pTTT->uFlags & TTF_IDISHWND) {
        nID = ::GetDlgCtrlID((HWND)nID);
    }

    if (nID != ID_VOLUME_MUTE) {
        return FALSE;
    }
    CToolBarCtrl& tb = GetToolBarCtrl();

    TBBUTTONINFO bi;
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_IMAGE;
    tb.GetButtonInfo(ID_VOLUME_MUTE, &bi);

    static CString strTipText;
    if (bi.iImage == 12) {
        strTipText.LoadString(ID_VOLUME_MUTE);
    } else if (bi.iImage == 13) {
        strTipText.LoadString(ID_VOLUME_MUTE_OFF);
    } else if (bi.iImage == 14) {
        strTipText.LoadString(ID_VOLUME_MUTE_DISABLED);
    } else {
        return FALSE;
    }
    pTTT->lpszText = (LPWSTR)(LPCWSTR)strTipText;

    *pResult = 0;


    return TRUE;    // message was handled
}

void CPlayerToolBar::OnLButtonUp(UINT nFlags, CPoint point)
{
    mouseDownL = false;
    CToolBar::OnLButtonUp(nFlags, point);
}

void CPlayerToolBar::OnRButtonUp(UINT nFlags, CPoint point) {
    CToolBar::OnRButtonUp(nFlags, point);
    mouseDownR = false;

    int buttonId = getHitButtonIdx(point);
    if (buttonId >= 0 && rightButtonIndex == buttonId) {
        int itemId = GetItemID(buttonId);

        UINT messageId = 0;

        switch (itemId) {
            case ID_PLAY_PLAY:
                messageId = ID_FILE_OPENMEDIA;
                break;
            case ID_PLAY_FRAMESTEP:
                messageId = ID_PLAY_FRAMESTEP_BACK;
                break;
            case ID_PLAY_STOP:
                messageId = ID_FILE_CLOSE_AND_RESTORE;
                break;
            case ID_NAVIGATE_SKIPFORWARD:
                messageId = ID_NAVIGATE_SKIPFORWARDFILE;
                break;
            case ID_NAVIGATE_SKIPBACK:
                messageId = ID_NAVIGATE_SKIPBACKFILE;
                break;
            case ID_VOLUME_MUTE:
                messageId = ID_STREAM_AUDIO_NEXT;
                break;
        }

        if (messageId > 0) {
            m_pMainFrame->PostMessage(WM_COMMAND, messageId);
        }
    }
}
