#pragma once
#include "pch.h"
#include "resource.h"
#include "PlanEntry.h"

#include <afxdialogex.h>

// ─────────────────────────────────────────────────────────────────────────────
//  "Status" — shows a plan's progress and its log, like the progress/log area
//  of the Universal File Copier.  Refreshes itself while a run is going on.
// ─────────────────────────────────────────────────────────────────────────────
class CPlanStatusDlg : public CDialogEx
{
public:
    CPlanStatusDlg(std::shared_ptr<CPlanEntry> entry, CWnd* pParent = nullptr);
    enum { IDD = IDD_PLAN_STATUS_DIALOG };

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnDestroy();
    afx_msg void OnBtnStop();
    DECLARE_MESSAGE_MAP()

private:
    std::shared_ptr<CPlanEntry> m_entry;
    size_t        m_nShown = 0;      // log lines already in the list box
    int           m_nRun   = -1;     // run serial the list box shows
    int           m_nMaxWidth = 0;   // for the horizontal scroll extent

    CStatic       m_staticTitle;
    CProgressCtrl m_progress;
    CStatic       m_staticStatus;
    CStatic       m_staticFile;
    CListBox      m_listLog;
    CButton       m_btnStop;

    void Refresh();
    void AppendLine(const CString& s);
};
