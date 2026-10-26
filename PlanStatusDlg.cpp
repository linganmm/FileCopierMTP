#include "pch.h"
#include "PlanStatusDlg.h"

static const UINT_PTR kRefreshTimer = 1;

BEGIN_MESSAGE_MAP(CPlanStatusDlg, CDialogEx)
    ON_WM_TIMER()
    ON_WM_DESTROY()
    ON_BN_CLICKED(IDC_BTN_CANCEL_COPY, &CPlanStatusDlg::OnBtnStop)
END_MESSAGE_MAP()

CPlanStatusDlg::CPlanStatusDlg(std::shared_ptr<CPlanEntry> entry, CWnd* pParent)
    : CDialogEx(IDD_PLAN_STATUS_DIALOG, pParent)
    , m_entry(std::move(entry))
{}

void CPlanStatusDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_STATIC_STATUS_TITLE, m_staticTitle);
    DDX_Control(pDX, IDC_PROGRESS,            m_progress);
    DDX_Control(pDX, IDC_STATIC_STATUS,       m_staticStatus);
    DDX_Control(pDX, IDC_STATIC_FILE,         m_staticFile);
    DDX_Control(pDX, IDC_LIST_LOG,            m_listLog);
    DDX_Control(pDX, IDC_BTN_CANCEL_COPY,     m_btnStop);
}

BOOL CPlanStatusDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    SetWindowText(_T("Status - ") + m_entry->plan.strTitle);
    m_staticTitle.SetWindowText(m_entry->plan.strFilePath);
    m_progress.SetRange(0, 100);
    Refresh();
    SetTimer(kRefreshTimer, 250, nullptr);
    return TRUE;
}

void CPlanStatusDlg::OnDestroy()
{
    KillTimer(kRefreshTimer);
    CDialogEx::OnDestroy();
}

void CPlanStatusDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == kRefreshTimer) Refresh();
    CDialogEx::OnTimer(nIDEvent);
}

void CPlanStatusDlg::OnBtnStop()
{
    if (m_entry->bRunning && m_entry->job)
    {
        m_entry->job->Cancel();
        m_staticStatus.SetWindowText(_T("Cancelling\u2026"));
    }
}

void CPlanStatusDlg::AppendLine(const CString& s)
{
    m_listLog.AddString(s);

    CClientDC dc(&m_listLog);
    CFont* pOld = dc.SelectObject(m_listLog.GetFont());
    int w = dc.GetTextExtent(s).cx + 8;
    dc.SelectObject(pOld);
    if (w > m_nMaxWidth)
    {
        m_nMaxWidth = w;
        m_listLog.SetHorizontalExtent(w);
    }
}

void CPlanStatusDlg::Refresh()
{
    const CPlanEntry& e = *m_entry;

    // Log — a new run clears the entry's log, so start over
    if (e.nRunSerial != m_nRun || e.log.size() < m_nShown)
    {
        m_nRun = e.nRunSerial;
        m_listLog.ResetContent();
        m_nShown = 0;
        m_nMaxWidth = 0;
        m_listLog.SetHorizontalExtent(0);
    }
    if (e.log.size() > m_nShown)
    {
        m_listLog.SetRedraw(FALSE);
        for (size_t i = m_nShown; i < e.log.size(); ++i)
            AppendLine(e.log[i]);
        m_nShown = e.log.size();
        m_listLog.SetRedraw(TRUE);
        m_listLog.Invalidate();
        int n = m_listLog.GetCount();
        if (n > 0) m_listLog.SetTopIndex(n - 1);
    }

    // Progress
    m_progress.SetPos(e.bRunning ? e.nPercent : (e.strLastRun.IsEmpty() ? 0 : 100));

    CString status;
    if (e.bRunning)
        status = e.strStatus.IsEmpty() ? CString(_T("Starting\u2026")) : e.strStatus;
    else if (!e.strLastRun.IsEmpty())
        status = e.strLastRun;
    else
        status = _T("This plan has not been run yet.");

    CString cur;
    m_staticStatus.GetWindowText(cur);
    if (cur != status) m_staticStatus.SetWindowText(status);

    CString file = e.bRunning ? e.strCurrentFile : CString();
    m_staticFile.GetWindowText(cur);
    if (cur != file) m_staticFile.SetWindowText(file);

    m_btnStop.EnableWindow(e.bRunning ? TRUE : FALSE);
}
