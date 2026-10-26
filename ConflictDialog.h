#pragma once
#include "pch.h"
#include "resource.h"

#include <afxwin.h>
#include <afxdialogex.h>

// Payload passed from the worker thread to the UI for conflict resolution.
// Mod times are in FILETIME (UTC 100-ns intervals since 1601-01-01).
// A zero FILETIME means "unknown / not available".
struct CConflictInfo
{
    CString  strRelPath;
    FILETIME ftSrc = {};   // modification time of the source file
    FILETIME ftDst = {};   // modification time of the destination file
    CString  strCaption;   // optional window caption (e.g. names the backup plan)
};

class CConflictDialog : public CDialogEx
{
    DECLARE_DYNAMIC(CConflictDialog)
public:
    CConflictDialog(const CConflictInfo& info, CWnd* pParent = nullptr);
    virtual ~CConflictDialog();
    enum { IDD = IDD_CONFLICT_DIALOG };

    int  m_nResolution;    // CONFLICT_OVERWRITE / CONFLICT_SKIP / CONFLICT_CANCEL
    BOOL m_bApplyToAll;

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX);
    afx_msg void OnBtnOverwrite();
    afx_msg void OnBtnSkip();
    afx_msg void OnBtnNewer();
    afx_msg void OnBtnCancel();
    DECLARE_MESSAGE_MAP()

private:
    CConflictInfo m_info;
    CButton m_chkApplyAll;
    CStatic m_staticMsg;

    static CString FormatFileTime(const FILETIME& ft);
};
