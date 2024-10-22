#pragma once
#include "CMPCThemeScrollBarHelper.h"
#include "CMPCThemeToolTipCtrl.h"
#include "CMPCThemeUtil.h"
#include "CMPCThemeHeaderCtrl.h"

//undocumented state changes for LVS_EX_CHECKBOXES
#define LVIS_UNCHECKED  0x1000
#define LVIS_CHECKED    0x2000

class CMPCThemeListCtrlCustomInterface
{
public:
    virtual void GetCustomTextColors(INT_PTR nItem, int iSubItem, COLORREF& clrText, COLORREF& clrTextBk, bool& overrideSelectedBG) = 0;
    virtual void DoCustomPrePaint() = 0;
    virtual void GetCustomGridColors(int nItem, COLORREF& horzGridColor, COLORREF& vertGridColor) = 0;
};

class CMPCThemePlayerListCtrl : public CListCtrl, CMPCThemeUtil, CMPCThemeScrollable
{
public:
    CMPCThemePlayerListCtrl();
    virtual ~CMPCThemePlayerListCtrl();
    DECLARE_DYNAMIC(CMPCThemePlayerListCtrl)

    void updateSB();
    void updateScrollInfo(bool invalidate = false);
    LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    void updateToolTip(CPoint point);
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    void setCheckedColors(COLORREF checkedBG, COLORREF checkedText, COLORREF uncheckedText);
    void subclassHeader();
    void setAdditionalStyles(DWORD styles);
    void setHasCBImages(bool on);
    void setItemTextWithDefaultFlag(int nItem, int nSubItem, LPCTSTR lpszText, bool flagged);
    void setFlaggedItem(int iItem, bool flagged);
    bool getFlaggedItem(int iItem);
    void setColorInterface(CMPCThemeListCtrlCustomInterface* iface) { customThemeInterface = iface; };
    void DoDPIChanged();

    DECLARE_MESSAGE_MAP()
    afx_msg void OnWindowPosChanged(WINDOWPOS* lpwndpos);
    afx_msg void OnNcPaint();
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg BOOL OnLvnEndScroll(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnNcCalcSize(BOOL bCalcValidRects, NCCALCSIZE_PARAMS* lpncsp);
    afx_msg BOOL OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
protected:
    CMPCThemeScrollBarHelper* themedSBHelper;
    CMPCThemeToolTipCtrl themedToolTip, lvsToolTip;
    UINT_PTR themedToolTipCid;
    COLORREF checkedBGClr, checkedTextClr, uncheckedTextClr;
    std::map<int, bool> flaggedItems;
    bool hasCheckedColors;
    bool hasCBImages;
    bool themeGridLines;
    bool fullRowSelect;
    CMPCThemeHeaderCtrl themedHdrCtrl;
    CFont* listMPCThemeFont, listMPCThemeFontBold;
    CMPCThemeListCtrlCustomInterface* customThemeInterface;
    void drawItem(CDC* pDC, int nItem, int nSubItem);
    virtual void PreSubclassWindow();
public:
    void doDefault() { Default(); };
    afx_msg BOOL OnHdnEndtrack(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg LRESULT OnDelayed_UpdateScrollbar(WPARAM, LPARAM);
    afx_msg BOOL OnLvnItemchanged(NMHDR* pNMHDR, LRESULT* pResult);
};

