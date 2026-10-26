#pragma once
#include "pch.h"
#include "resource.h"
#include "BackupPlan.h"
#include "MtpBrowserDlg.h"

#include <afxdialogex.h>

// ─────────────────────────────────────────────────────────────────────────────
//  "Add Item" / "Edit Item" — picks the source files/folders, optional
//  exclusions (files/folders inside the source that are not copied) and the
//  destination folder of one backup-plan item, using the same device/folder
//  browser as the Universal File Copier.
// ─────────────────────────────────────────────────────────────────────────────
class CItemDlg : public CDialogEx
{
public:
    // The browser states are owned by the plan dialog so the device trees
    // stay expanded between items.
    CItemDlg(CBrowserState* pSrcState, CBrowserState* pExclState,
             CBrowserState* pDstState, bool bEditMode, CWnd* pParent = nullptr);
    enum { IDD = IDD_ITEM_DIALOG };

    CPlanItem m_item;          // in: item being edited; out: result on IDOK

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual void OnOK();

    afx_msg void OnBtnBrowseSrc();
    afx_msg void OnBtnBrowseDst();
    afx_msg void OnBtnBrowseExcl();
    afx_msg void OnBtnClearExcl();
    DECLARE_MESSAGE_MAP()

private:
    CBrowserState* m_pSrcState;
    CBrowserState* m_pExclState;
    CBrowserState* m_pDstState;
    bool           m_bEditMode;
    bool           m_bSrcBrowsed = false;

    CEdit   m_editSrc;
    CEdit   m_editDst;
    CStatic m_staticSrcType;
    CStatic m_staticDstType;
    CEdit   m_editExcl;
    CStatic m_staticExclType;
    CComboBox m_comboConflict;

    void UpdateUi();
    static CString DeviceName(const CBrowserState* pState, const CString& deviceId);
    static bool    IsInsideSource(const CEndpoint& src, const CSourceItem& x);
    void           DropOutsideExclusions(bool bTellUser);
    static CString StateDeviceId(const CBrowserState* pState);
    static bool    SourceOk(const CEndpoint& ep);
    static bool    DestOk(const CEndpoint& ep);
};
