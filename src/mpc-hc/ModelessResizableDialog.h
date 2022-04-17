#pragma once
#include "CMPCThemeResizableDialog.h"
class CModelessResizableDialog : public CMPCThemeResizableDialog {
public:
    CModelessResizableDialog(UINT nIDTemplate, CWnd* pParent);
    virtual BOOL Create(UINT nIDTemplate, CWnd* pParentWnd, bool showOnWindowPlacement = true);
    void HideDialog(INT_PTR ret);
    DECLARE_MESSAGE_MAP()
protected:
    virtual void OnOK();
    virtual void OnCancel();
};

