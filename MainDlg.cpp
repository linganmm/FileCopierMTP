#include "pch.h"
#include "MainDlg.h"
#include "MtpBrowserDlg.h"
#include "ConflictDialog.h"
#include <filesystem>
namespace fs = std::filesystem;

BEGIN_MESSAGE_MAP(CMainDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_BROWSE_SRC,   &CMainDlg::OnBtnBrowseSrc)
    ON_BN_CLICKED(IDC_BTN_BROWSE_DST,   &CMainDlg::OnBtnBrowseDst)
    ON_BN_CLICKED(IDC_BTN_START,         &CMainDlg::OnBtnStart)
    ON_BN_CLICKED(IDC_BTN_CANCEL_COPY,   &CMainDlg::OnBtnCancelCopy)
    ON_MESSAGE(WM_COPY_PROGRESS,         &CMainDlg::OnCopyProgress)
    ON_MESSAGE(WM_COPY_CONFLICT,         &CMainDlg::OnCopyConflict)
    ON_MESSAGE(WM_COPY_DONE,             &CMainDlg::OnCopyDone)
    ON_MESSAGE(WM_COPY_ERROR,            &CMainDlg::OnCopyError)
    ON_MESSAGE(WM_COPY_LOG,              &CMainDlg::OnCopyLog)
END_MESSAGE_MAP()

CMainDlg::CMainDlg(CWnd* pParent) : CDialogEx(IDD_MAIN_DIALOG, pParent) {}
CMainDlg::~CMainDlg()
{
    if (m_job) m_job->Cancel();   // the worker keeps the job alive until it exits
}

void CMainDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_SRC_PATH,   m_editSrcPath);
    DDX_Control(pDX, IDC_EDIT_DST_PATH,   m_editDstPath);
    DDX_Control(pDX, IDC_STATIC_SRC_TYPE, m_staticSrcType);
    DDX_Control(pDX, IDC_STATIC_DST_TYPE, m_staticDstType);
    DDX_Control(pDX, IDC_PROGRESS,        m_progress);
    DDX_Control(pDX, IDC_STATIC_STATUS,   m_staticStatus);
    DDX_Control(pDX, IDC_STATIC_FILE,     m_staticFile);
    DDX_Control(pDX, IDC_LIST_LOG,        m_listLog);
    DDX_Control(pDX, IDC_BTN_START,       m_btnStart);
    DDX_Control(pDX, IDC_BTN_CANCEL_COPY, m_btnCancel);
}

BOOL CMainDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    SetWindowText(_T("Universal File Copier"));
    m_progress.SetRange(0, 100);
    m_progress.SetPos(0);
    m_staticStatus.SetWindowText(
        _T("Select source items and a destination folder, then click Start Copy."));
    m_editSrcPath.SetReadOnly(TRUE);
    m_editDstPath.SetReadOnly(TRUE);
    m_btnCancel.EnableWindow(FALSE);
    m_btnStart.EnableWindow(FALSE);

    // Create persistent browser dialogs now that our HWND exists.
    // They are shown/hidden on each Browse click rather than recreated.
    m_srcDlg = std::make_unique<CMtpBrowserDlg>(this);
    m_srcDlg->m_bSourceMode = true;

    m_dstDlg = std::make_unique<CMtpBrowserDlg>(this);
    m_dstDlg->m_bSourceMode = false;

    return TRUE;
}

// ── Browse source — multi-select mode ────────────────────────────────────────

void CMainDlg::OnBtnBrowseSrc()
{
	m_srcDlg->m_pState = &m_srcState;  // share persistent state with dialog
    if (m_srcDlg->DoModal() != IDOK) return;

    m_src.bIsLocal        = m_srcDlg->m_bIsLocalDisk;
    m_src.strDeviceId     = m_srcDlg->m_strSelectedDeviceId;
    m_src.items           = m_srcDlg->m_selectedItems;
    m_src.strDisplayPath  = m_srcDlg->m_strSelectedDisplayPath;

    m_editSrcPath.SetWindowText(m_src.strDisplayPath);
    m_staticSrcType.SetWindowText(m_src.bIsLocal ? _T("[Local]") : _T("[MTP]"));
    UpdateStartButton();
}

// ── Browse destination — single folder ───────────────────────────────────────

void CMainDlg::OnBtnBrowseDst()
{
    m_dstDlg->m_pState = &m_dstState;
    if (m_dstDlg->DoModal() != IDOK) return;

    m_dst.bIsLocal        = m_dstDlg->m_bIsLocalDisk;
    m_dst.strDeviceId     = m_dstDlg->m_strSelectedDeviceId;
    m_dst.strDisplayPath  = m_dstDlg->m_strSelectedDisplayPath;

    if (m_dst.bIsLocal)
    {
        m_dst.strLocalPath = m_dstDlg->m_strSelectedLocalPath;
        m_dst.strObjectId.Empty();
    }
    else
    {
        m_dst.strObjectId  = m_dstDlg->m_strSelectedObjectId;
        m_dst.strLocalPath.Empty();
    }

    m_editDstPath.SetWindowText(m_dst.strDisplayPath);
    m_staticDstType.SetWindowText(m_dst.bIsLocal ? _T("[Local]") : _T("[MTP]"));
    UpdateStartButton();
}

void CMainDlg::UpdateStartButton()
{
    bool srcOk = !m_src.items.empty();
    bool dstOk = m_dst.bIsLocal ? !m_dst.strLocalPath.IsEmpty()
                                 : (!m_dst.strDeviceId.IsEmpty() && !m_dst.strObjectId.IsEmpty());
    m_btnStart.EnableWindow(srcOk && dstOk);
}

// ── Start copy ────────────────────────────────────────────────────────────────

void CMainDlg::OnBtnStart()
{
    if (m_src.items.empty())
    {
        MessageBox(_T("Please select source files/folders."), _T("Error"), MB_ICONWARNING);
        return;
    }
    if (!m_dst.HasDest())
    {
        MessageBox(_T("Please select a destination folder."), _T("Error"), MB_ICONWARNING);
        return;
    }

    // Validate / create local destination
    if (m_dst.bIsLocal && !fs::exists(m_dst.strLocalPath.GetString()))
    {
        if (MessageBox(_T("Destination folder does not exist. Create it?"),
                       _T("Confirm"), MB_YESNO | MB_ICONQUESTION) == IDYES)
        {
            std::error_code ec;
            fs::create_directories(m_dst.strLocalPath.GetString(), ec);
            if (ec) { MessageBox(_T("Could not create destination."), _T("Error"), MB_ICONERROR); return; }
        }
        else return;
    }

    std::vector<CCopyTask> tasks{ CCopyTask{ m_src, m_dst } };
    m_job = std::make_shared<CCopyJob>(GetSafeHwnd(), std::move(tasks), false);

    m_listLog.ResetContent();
    m_progress.SetPos(0);
    m_staticStatus.SetWindowText(_T("Starting\u2026"));
    SetControlsEnabled(TRUE);

    if (!CCopyJob::Start(m_job))
    {
        SetControlsEnabled(FALSE);
        m_staticStatus.SetWindowText(_T("Could not start the copy thread."));
    }
}

void CMainDlg::OnBtnCancelCopy()
{
    if (m_job) m_job->Cancel();
    m_staticStatus.SetWindowText(_T("Cancelling\u2026"));
}

void CMainDlg::OnCancel()
{
    if (m_job && m_job->IsRunning())
    {
        if (MessageBox(_T("A copy is in progress. Cancel it and close?"),
                       _T("Universal File Copier"), MB_YESNO | MB_ICONQUESTION) != IDYES)
            return;
        m_job->Cancel();
    }
    CDialogEx::OnCancel();
}

// ── UI message handlers ───────────────────────────────────────────────────────

LRESULT CMainDlg::OnCopyProgress(WPARAM wParam, LPARAM lParam)
{
    std::unique_ptr<CProgressInfo> info(reinterpret_cast<CProgressInfo*>(lParam));
    if (info && IsMyJob(wParam))
    {
        int pct = info->nTotal > 0 ? (int)(100LL * info->nCurrent / info->nTotal) : 0;
        m_progress.SetPos(pct);
        CString s;
        s.Format(_T("Copying %d of %d  (%d%%)"), info->nCurrent, info->nTotal, pct);
        m_staticStatus.SetWindowText(s);
        m_staticFile.SetWindowText(info->strCurrentFile);
    }
    return 0;
}

LRESULT CMainDlg::OnCopyConflict(WPARAM wParam, LPARAM lParam)
{
    std::unique_ptr<CString> pPath(reinterpret_cast<CString*>(lParam));
    if (!IsMyJob(wParam)) return 0;
    std::shared_ptr<CCopyJob> job = m_job;   // keep alive across the modal loop

    CConflictInfo info;
    info.strRelPath = pPath ? *pPath : CString(_T("(unknown)"));
    job->GetConflictTimes(info.ftSrc, info.ftDst);

    CConflictDialog dlg(info, this);
    dlg.DoModal();
    job->SetConflictResolution(dlg.m_nResolution, dlg.m_bApplyToAll != FALSE);
    return 0;
}

LRESULT CMainDlg::OnCopyDone(WPARAM wParam, LPARAM lParam)
{
    std::unique_ptr<CDoneInfo> done(reinterpret_cast<CDoneInfo*>(lParam));
    if (!IsMyJob(wParam)) return 0;
    SetControlsEnabled(FALSE);
    UpdateStartButton();
    m_progress.SetPos(100);
    bool cancelled = done ? done->bCancelled : true;
    m_staticStatus.SetWindowText(cancelled ? _T("Cancelled.") : _T("Done!"));
    m_staticFile.SetWindowText(_T(""));
    if (done) AddLog(done->strSummary);
    return 0;
}

LRESULT CMainDlg::OnCopyError(WPARAM wParam, LPARAM lParam)
{
    std::unique_ptr<CString> pMsg(reinterpret_cast<CString*>(lParam));
    if (pMsg && IsMyJob(wParam)) AddLog(_T("[ERR] ") + *pMsg);
    return 0;
}

LRESULT CMainDlg::OnCopyLog(WPARAM wParam, LPARAM lParam)
{
    std::unique_ptr<CString> pMsg(reinterpret_cast<CString*>(lParam));
    if (pMsg && IsMyJob(wParam)) AddLog(*pMsg);
    return 0;
}

void CMainDlg::SetControlsEnabled(BOOL bCopying)
{
    GetDlgItem(IDC_BTN_BROWSE_SRC)->EnableWindow(!bCopying);
    GetDlgItem(IDC_BTN_BROWSE_DST)->EnableWindow(!bCopying);
    m_btnStart.EnableWindow(!bCopying);
    m_btnCancel.EnableWindow(bCopying);
}

void CMainDlg::AddLog(const CString& msg)
{
    m_listLog.AddString(msg);
    int n = m_listLog.GetCount();
    if (n > 0) m_listLog.SetTopIndex(n - 1);
}
