/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2020 see Authors.txt
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

#pragma once

class CMainFrame;

// CPreView

class previewView : public CWnd {
    DECLARE_DYNAMIC(previewView)
    DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    bool onePaint = false;
private:
    CMPCPngImage noImage;
};

class CPreView : public CWnd {
    DECLARE_DYNAMIC(CPreView)

private:

    CMainFrame* m_pMainFrame;

    int m_border = 1;
    int m_caption = 20;
    int m_relativeSize = 15;

    CString	m_tooltipstr;
    previewView	m_view;
    CRect	m_videorect;

    CFont m_font;

    struct t_color {
        int R1, G1, B1;
        int R2, G2, B2;
    };
    COLORREF m_crText;

public:
    CPreView(CMainFrame* pMainFrame);
    virtual ~CPreView() = default;

    virtual BOOL SetWindowTextW(LPCWSTR lpString);

    void SetRelativeSize(int relativesize) { m_relativeSize = relativesize; }
    void GetVideoRect(LPRECT lpRect);
    HWND GetVideoHWND();

    void SetWindowSize();

    void ScaleFont();
    void SetColor();

protected:
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnPaint();
    afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);

    DECLARE_MESSAGE_MAP()
};
