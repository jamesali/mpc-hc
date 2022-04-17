#include "stdafx.h"
#include "CMPCThemeResizableDialog.h"
#include "CMPCTheme.h"
#include "mplayerc.h"

CMPCThemeResizableDialog::CMPCThemeResizableDialog()
{
}

CMPCThemeResizableDialog::CMPCThemeResizableDialog(UINT nIDTemplate, CWnd* pParent): CResizableDialog(nIDTemplate, pParent)
{
}

CMPCThemeResizableDialog::CMPCThemeResizableDialog(LPCTSTR lpszTemplateName, CWnd* pParent): CResizableDialog(lpszTemplateName, pParent)
{
}


CMPCThemeResizableDialog::~CMPCThemeResizableDialog()
{
}

BOOL CMPCThemeResizableDialog::OnInitDialog() {
    BOOL ret = CResizableDialog::OnInitDialog();
    CMPCThemeUtil::enableWindows10DarkFrame(this);
    return ret;
}

void CMPCThemeResizableDialog::fulfillThemeReqs()
{
    CMPCThemeUtil::enableWindows10DarkFrame(this);
    if (AppIsThemeLoaded()) {
        SetSizeGripBkMode(TRANSPARENT); //fix for gripper in mpc theme
    }
    CMPCThemeUtil::fulfillThemeReqs((CWnd*)this);
}

BEGIN_MESSAGE_MAP(CMPCThemeResizableDialog, CResizableDialog)
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()


HBRUSH CMPCThemeResizableDialog::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    if (AppIsThemeLoaded()) {
        return getCtlColor(pDC, pWnd, nCtlColor);
    } else {
        HBRUSH hbr = __super::OnCtlColor(pDC, pWnd, nCtlColor);
        return hbr;
    }
}
