#pragma once
#include "pch.h"
#include "resource.h"
#include "WpdHelper.h"
#include "MtpBrowserDlg.h"

#include <afxwin.h>
#include <afxdialogex.h>

#include "CopyEngine.h"

// ─────────────────────────────────────────────────────────────────────────────
class CMainDlg : public CDialogEx
{
public:
    CMainDlg(CWnd* pParent = nullptr);
    virtual ~CMainDlg();
    enum { IDD = IDD_MAIN_DIALOG };

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual void OnCancel();

    afx_msg void OnBtnBrowseSrc();
    afx_msg void OnBtnBrowseDst();
    afx_msg void OnBtnStart();
    afx_msg void OnBtnCancelCopy();

    afx_msg LRESULT OnCopyProgress(WPARAM, LPARAM);
    afx_msg LRESULT OnCopyConflict(WPARAM, LPARAM);
    afx_msg LRESULT OnCopyDone(WPARAM, LPARAM);
    afx_msg LRESULT OnCopyError(WPARAM, LPARAM);
    afx_msg LRESULT OnCopyLog(WPARAM, LPARAM);

    DECLARE_MESSAGE_MAP()

private:
    CEdit         m_editSrcPath;
    CEdit         m_editDstPath;
    CStatic       m_staticSrcType;
    CStatic       m_staticDstType;
    CProgressCtrl m_progress;
    CStatic       m_staticStatus;
    CStatic       m_staticFile;
    CListBox      m_listLog;
    CButton       m_btnStart;
    CButton       m_btnCancel;

    CEndpoint     m_src;   // source endpoint — items[] is populated
    CEndpoint     m_dst;   // destination endpoint — strLocalPath/strObjectId used

    std::shared_ptr<CCopyJob> m_job;   // running / last copy job

    std::unique_ptr<CMtpBrowserDlg> m_srcDlg;
    std::unique_ptr<CMtpBrowserDlg> m_dstDlg;
    // Stored here (not inside CMtpBrowserDlg) so it survives across DoModal
    // calls without keeping a CWnd-derived object alive between invocations.
    CBrowserState m_srcState;
    CBrowserState m_dstState;

    void SetControlsEnabled(BOOL bCopying);
    void AddLog(const CString& msg);
    void UpdateStartButton();
    bool IsMyJob(WPARAM wParam) const { return m_job && m_job->GetId() == (UINT)wParam; }
};
