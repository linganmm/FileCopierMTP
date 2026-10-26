#include "pch.h"
#include "LXFreeBackView.h"
#include "PlanDlg.h"
#include "PlanStatusDlg.h"
#include "MainDlg.h"
#include "ConflictDialog.h"

IMPLEMENT_DYNCREATE(CLXFreeBackView, CView)

BEGIN_MESSAGE_MAP(CLXFreeBackView, CView)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_SETFOCUS()
    ON_WM_ERASEBKGND()

    ON_COMMAND(ID_FILE_NEW_PLAN,   &CLXFreeBackView::OnFileNewPlan)
    ON_COMMAND(ID_FILE_LOAD_PLAN,  &CLXFreeBackView::OnFileLoadPlan)
    ON_COMMAND(ID_FILE_QUICK_COPY, &CLXFreeBackView::OnFileQuickCopy)

    ON_COMMAND(ID_TOOL_EDIT_PLAN,   &CLXFreeBackView::OnToolEdit)
    ON_COMMAND(ID_TOOL_UNLOAD_PLAN, &CLXFreeBackView::OnToolUnload)
    ON_COMMAND(ID_TOOL_REMOVE_PLAN, &CLXFreeBackView::OnToolRemove)
    ON_COMMAND(ID_TOOL_RUN_PLAN,    &CLXFreeBackView::OnToolRun)
    ON_COMMAND(ID_TOOL_STATUS_PLAN, &CLXFreeBackView::OnToolStatus)
    ON_UPDATE_COMMAND_UI(ID_TOOL_EDIT_PLAN,   &CLXFreeBackView::OnUpdateToolEdit)
    ON_UPDATE_COMMAND_UI(ID_TOOL_UNLOAD_PLAN, &CLXFreeBackView::OnUpdateToolUnload)
    ON_UPDATE_COMMAND_UI(ID_TOOL_REMOVE_PLAN, &CLXFreeBackView::OnUpdateToolRemove)
    ON_UPDATE_COMMAND_UI(ID_TOOL_RUN_PLAN,    &CLXFreeBackView::OnUpdateToolRun)
    ON_UPDATE_COMMAND_UI(ID_TOOL_STATUS_PLAN, &CLXFreeBackView::OnUpdateToolStatus)

    ON_MESSAGE(WM_ACTIONLIST_BUTTON, &CLXFreeBackView::OnListButton)
    ON_MESSAGE(WM_ACTIONLIST_DBLCLK, &CLXFreeBackView::OnListDblClk)

    ON_MESSAGE(WM_COPY_PROGRESS, &CLXFreeBackView::OnCopyProgress)
    ON_MESSAGE(WM_COPY_CONFLICT, &CLXFreeBackView::OnCopyConflict)
    ON_MESSAGE(WM_COPY_DONE,     &CLXFreeBackView::OnCopyDone)
    ON_MESSAGE(WM_COPY_ERROR,    &CLXFreeBackView::OnCopyError)
    ON_MESSAGE(WM_COPY_LOG,      &CLXFreeBackView::OnCopyLog)
END_MESSAGE_MAP()

CLXFreeBackView::CLXFreeBackView() {}
CLXFreeBackView::~CLXFreeBackView() {}

CLXFreeBackDoc* CLXFreeBackView::GetDocument() const
{
    return reinterpret_cast<CLXFreeBackDoc*>(m_pDocument);
}

std::vector<std::shared_ptr<CPlanEntry>>& CLXFreeBackView::Plans() const
{
    return GetDocument()->m_plans;
}

BOOL CLXFreeBackView::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CView::PreCreateWindow(cs)) return FALSE;
    cs.style     |= WS_CLIPCHILDREN;
    cs.dwExStyle &= ~WS_EX_CLIENTEDGE;     // the list draws its own border
    return TRUE;
}

void CLXFreeBackView::OnDraw(CDC* /*pDC*/)
{
    // The list control covers the whole client area.
}

BOOL CLXFreeBackView::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;
}

int CLXFreeBackView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CView::OnCreate(lpCreateStruct) == -1)
        return -1;

    m_list.SetButtonLabels({ _T("Edit"), _T("Unload"), _T("Remove"), _T("Run"), _T("Status") });
    m_list.SetEmptyText(_T("No backup plans loaded.\n\n")
                        _T("Use File \u2192 New Backup Plan to create one, or\n")
                        _T("File \u2192 Load Backup Plan to open an existing .lbp file."));
    if (!m_list.Create(CRect(0, 0, 0, 0), this, IDC_PLAN_LIST))
        return -1;
    return 0;
}

void CLXFreeBackView::OnSize(UINT nType, int cx, int cy)
{
    CView::OnSize(nType, cx, cy);
    if (m_list.GetSafeHwnd())
        m_list.MoveWindow(0, 0, cx, cy);
}

void CLXFreeBackView::OnSetFocus(CWnd* pOldWnd)
{
    CView::OnSetFocus(pOldWnd);
    if (m_list.GetSafeHwnd()) m_list.SetFocus();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

CString CLXFreeBackView::DateTimeNow()
{
    SYSTEMTIME st;
    ::GetLocalTime(&st);
    CString s;
    s.Format(_T("%04u-%02u-%02u %02u:%02u:%02u"),
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return s;
}

int CLXFreeBackView::SelectedPlan() const
{
    int i = m_list.GetSel();
    return (i >= 0 && i < (int)Plans().size()) ? i : -1;
}

bool CLXFreeBackView::IsRunning(int index) const
{
    return index >= 0 && index < (int)Plans().size() && Plans()[index]->bRunning;
}

int CLXFreeBackView::IndexOf(const std::shared_ptr<CPlanEntry>& entry) const
{
    for (int i = 0; i < (int)Plans().size(); ++i)
        if (Plans()[i] == entry) return i;
    return -1;
}

void CLXFreeBackView::SetStatusText(const CString& text)
{
    CFrameWnd* pFrame = GetParentFrame();
    if (pFrame) pFrame->SetMessageText(text);
}

void CLXFreeBackView::RefreshRow(int index)
{
    if (index < 0 || index >= (int)Plans().size()) return;
    const CPlanEntry& e = *Plans()[index];

    CString sub;
    int n = (int)e.plan.items.size();
    if (!e.strLastRun.IsEmpty())
        sub = e.strLastRun + _T("     ") + e.plan.strFilePath;
    else
        sub.Format(_T("%d item%s  \u00b7  %s"), n, n == 1 ? _T("") : _T("s"),
                   (LPCTSTR)e.plan.strFilePath);
    m_list.SetRowText(index, e.plan.strTitle, sub);

    m_list.EnableButton(index, BTN_EDIT,   !e.bRunning);
    m_list.EnableButton(index, BTN_UNLOAD, !e.bRunning);
    m_list.EnableButton(index, BTN_REMOVE, !e.bRunning);
    m_list.SetButtonText(index, BTN_RUN, e.bRunning ? _T("Stop") : _T("Run"));

    CString caption;
    caption.Format(_T("%d%%   %s"), e.nPercent, (LPCTSTR)e.strStatus);
    m_list.SetRowProgress(index, e.bRunning, e.nPercent, caption);
}

void CLXFreeBackView::AddEntry(const CBackupPlan& plan)
{
    auto e = std::make_shared<CPlanEntry>();
    e->plan = plan;
    Plans().push_back(e);
    int i = m_list.AddRow(plan.strTitle, _T(""));
    RefreshRow(i);
    m_list.SetSel(i);
}

// A plan saved over the file of another loaded plan replaces that entry.
void CLXFreeBackView::RemoveDuplicatesOf(const std::shared_ptr<CPlanEntry>& keep)
{
    for (int j = (int)Plans().size() - 1; j >= 0; --j)
    {
        auto& other = Plans()[j];
        if (other != keep && !other->bRunning &&
            other->plan.strFilePath.CompareNoCase(keep->plan.strFilePath) == 0)
        {
            Plans().erase(Plans().begin() + j);
            m_list.DeleteRow(j);
        }
    }
}

// ── File menu ─────────────────────────────────────────────────────────────────

void CLXFreeBackView::OnFileNewPlan()
{
    CBackupPlan plan;
    CPlanDlg dlg(plan, false, this);
    dlg.DoModal();
    if (!dlg.m_bSaved) return;         // never saved → nothing to show

    AddEntry(dlg.m_savedPlan);
    auto e = Plans().back();
    RemoveDuplicatesOf(e);
    m_list.SetSel(IndexOf(e));
    SetStatusText(_T("Saved backup plan ") + e->plan.strFilePath);
}

bool CLXFreeBackView::LoadPlanFile(const CString& path, bool bReportErrors)
{
    TCHAR full[4096] = {};
    CString fullPath = ::GetFullPathName(path, 4096, full, nullptr) ? CString(full) : path;

    int existing = GetDocument()->FindByPath(fullPath);
    if (existing >= 0)
    {
        m_list.SetSel(existing);
        SetStatusText(_T("Already loaded: ") + fullPath);
        return true;
    }

    CBackupPlan plan;
    CString err;
    if (!PlanFile::Load(fullPath, plan, err))
    {
        if (bReportErrors)
            MessageBox(err, _T("Load Backup Plan"), MB_ICONERROR);
        return false;
    }
    AddEntry(plan);
    SetStatusText(_T("Loaded backup plan ") + fullPath);
    return true;
}

void CLXFreeBackView::OnFileLoadPlan()
{
    CString exeDir = PlanFile::GetExeDir();
    CFileDialog dlg(TRUE, _T("lbp"), nullptr,
                    OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY |
                    OFN_ALLOWMULTISELECT,
                    _T("LXFreeBack Backup Plans (*.lbp)|*.lbp|All Files (*.*)|*.*||"),
                    this);
    std::vector<TCHAR> buf(64 * 1024, 0);
    dlg.m_ofn.lpstrFile       = buf.data();
    dlg.m_ofn.nMaxFile        = (DWORD)buf.size();
    dlg.m_ofn.lpstrInitialDir = exeDir;
    dlg.m_ofn.lpstrTitle      = _T("Load Backup Plan");
    if (dlg.DoModal() != IDOK) return;

    POSITION pos = dlg.GetStartPosition();
    while (pos)
        LoadPlanFile(dlg.GetNextPathName(pos), true);
}

void CLXFreeBackView::OnFileQuickCopy()
{
    CMainDlg dlg(this);
    dlg.DoModal();
}

// ── Plan actions ──────────────────────────────────────────────────────────────

void CLXFreeBackView::EditPlan(int index)
{
    if (index < 0 || index >= (int)Plans().size() || IsRunning(index)) return;
    auto e = Plans()[index];

    CPlanDlg dlg(e->plan, true, this);
    dlg.DoModal();
    if (!dlg.m_bSaved) return;

    e->plan = dlg.m_savedPlan;
    RemoveDuplicatesOf(e);
    int i = IndexOf(e);
    RefreshRow(i);
    m_list.SetSel(i);
    SetStatusText(_T("Saved backup plan ") + e->plan.strFilePath);
}

void CLXFreeBackView::UnloadPlan(int index)
{
    if (index < 0 || index >= (int)Plans().size() || IsRunning(index)) return;
    CString title = Plans()[index]->plan.strTitle;
    Plans().erase(Plans().begin() + index);
    m_list.DeleteRow(index);
    SetStatusText(_T("Unloaded ") + title);
}

void CLXFreeBackView::RemovePlan(int index)
{
    if (index < 0 || index >= (int)Plans().size() || IsRunning(index)) return;
    const CBackupPlan& plan = Plans()[index]->plan;

    CString q;
    q.Format(_T("Remove the backup plan \"%s\"?\n\n")
             _T("The plan file will be deleted from disk:\n%s\n\n")
             _T("Files that were already backed up are not affected."),
             (LPCTSTR)plan.strTitle, (LPCTSTR)plan.strFilePath);
    if (MessageBox(q, _T("Remove Backup Plan"),
                   MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        return;

    if (!plan.strFilePath.IsEmpty() && !::DeleteFile(plan.strFilePath))
    {
        DWORD err = ::GetLastError();
        if (err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND)
        {
            MessageBox(_T("Could not delete ") + plan.strFilePath + _T(":\n") +
                       PlanFile::ErrorText(err),
                       _T("Remove Backup Plan"), MB_ICONERROR);
            return;
        }
    }
    CString title = plan.strTitle;
    Plans().erase(Plans().begin() + index);
    m_list.DeleteRow(index);
    SetStatusText(_T("Removed ") + title);
}

void CLXFreeBackView::RunPlan(int index)
{
    if (m_bClosing || index < 0 || index >= (int)Plans().size()) return;
    auto e = Plans()[index];

    if (e->bRunning)                       // the Run button reads "Stop"
    {
        if (e->job) e->job->Cancel();
        e->strStatus = _T("Stopping\u2026");
        RefreshRow(index);
        return;
    }
    if (e->plan.items.empty())
    {
        MessageBox(_T("This backup plan has no items yet.\nUse Edit to add some."),
                   _T("Run Backup Plan"), MB_ICONINFORMATION);
        return;
    }

    e->job = std::make_shared<CCopyJob>(GetSafeHwnd(), PlanFile::ToTasks(e->plan), true);
    e->log.clear();
    ++e->nRunSerial;
    e->nPercent = 0;
    e->strStatus = _T("Starting\u2026");
    e->strCurrentFile.Empty();
    e->log.push_back(_T("Run started ") + DateTimeNow());

    if (!CCopyJob::Start(e->job))
    {
        e->log.push_back(_T("[ERR] Could not start the copy thread."));
        MessageBox(_T("Could not start the copy thread."), _T("Run Backup Plan"), MB_ICONERROR);
        return;
    }
    e->bRunning = true;
    RefreshRow(index);
    SetStatusText(_T("Running ") + e->plan.strTitle);
}

void CLXFreeBackView::ShowStatus(int index)
{
    if (index < 0 || index >= (int)Plans().size()) return;
    CPlanStatusDlg dlg(Plans()[index], this);
    dlg.DoModal();
}

// ── Row buttons / Tool menu ───────────────────────────────────────────────────

LRESULT CLXFreeBackView::OnListButton(WPARAM wParam, LPARAM)
{
    int row = (int)LOWORD(wParam);
    switch ((int)HIWORD(wParam))
    {
    case BTN_EDIT:   EditPlan(row);   break;
    case BTN_UNLOAD: UnloadPlan(row); break;
    case BTN_REMOVE: RemovePlan(row); break;
    case BTN_RUN:    RunPlan(row);    break;
    case BTN_STATUS: ShowStatus(row); break;
    }
    return 0;
}

LRESULT CLXFreeBackView::OnListDblClk(WPARAM wParam, LPARAM)
{
    ShowStatus((int)wParam);
    return 0;
}

void CLXFreeBackView::OnToolEdit()   { EditPlan(SelectedPlan()); }
void CLXFreeBackView::OnToolUnload() { UnloadPlan(SelectedPlan()); }
void CLXFreeBackView::OnToolRemove() { RemovePlan(SelectedPlan()); }
void CLXFreeBackView::OnToolRun()    { RunPlan(SelectedPlan()); }
void CLXFreeBackView::OnToolStatus() { ShowStatus(SelectedPlan()); }

void CLXFreeBackView::OnUpdateToolEdit(CCmdUI* pCmdUI)
{
    int i = SelectedPlan();
    pCmdUI->Enable(i >= 0 && !IsRunning(i));
}

void CLXFreeBackView::OnUpdateToolUnload(CCmdUI* pCmdUI)
{
    int i = SelectedPlan();
    pCmdUI->Enable(i >= 0 && !IsRunning(i));
}

void CLXFreeBackView::OnUpdateToolRemove(CCmdUI* pCmdUI)
{
    int i = SelectedPlan();
    pCmdUI->Enable(i >= 0 && !IsRunning(i));
}

void CLXFreeBackView::OnUpdateToolRun(CCmdUI* pCmdUI)
{
    int i = SelectedPlan();
    pCmdUI->Enable(i >= 0 && !m_bClosing);
    pCmdUI->SetText(IsRunning(i) ? _T("&Stop") : _T("&Run"));
}

void CLXFreeBackView::OnUpdateToolStatus(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(SelectedPlan() >= 0);
}

// ── Copy-job notifications ────────────────────────────────────────────────────

LRESULT CLXFreeBackView::OnCopyProgress(WPARAM wParam, LPARAM lParam)
{
    std::unique_ptr<CProgressInfo> info(reinterpret_cast<CProgressInfo*>(lParam));
    int i = GetDocument()->FindByJob((UINT)wParam);
    if (!info || i < 0) return 0;
    CPlanEntry& e = *Plans()[i];
    if (!e.bRunning) return 0;

    e.nPercent = info->OverallPercent();
    if (info->nTotal > 0)
        e.strStatus.Format(_T("Item %d/%d  \u00b7  copying %d of %d"),
                           info->nTask, info->nTaskCount, info->nCurrent, info->nTotal);
    else
        e.strStatus.Format(_T("Item %d/%d  \u00b7  preparing\u2026"),
                           info->nTask, info->nTaskCount);
    e.strCurrentFile = info->strCurrentFile;

    CString caption;
    caption.Format(_T("%d%%   %s"), e.nPercent, (LPCTSTR)e.strStatus);
    m_list.SetRowProgress(i, true, e.nPercent, caption);
    return 0;
}

LRESULT CLXFreeBackView::OnCopyConflict(WPARAM wParam, LPARAM lParam)
{
    std::unique_ptr<CString> pPath(reinterpret_cast<CString*>(lParam));
    int i = GetDocument()->FindByJob((UINT)wParam);
    if (i < 0) return 0;
    auto e   = Plans()[i];
    auto job = e->job;
    if (!job) return 0;

    if (m_bClosing || job->IsCancelled())
    {
        job->SetConflictResolution(CONFLICT_CANCEL, false);
        return 0;
    }

    CConflictInfo info;
    info.strRelPath = pPath ? *pPath : CString(_T("(unknown)"));
    info.strCaption = _T("File Conflict - ") + e->plan.strTitle;
    job->GetConflictTimes(info.ftSrc, info.ftDst);

    CConflictDialog dlg(info, this);
    dlg.DoModal();
    job->SetConflictResolution(dlg.m_nResolution, dlg.m_bApplyToAll != FALSE);
    return 0;
}

LRESULT CLXFreeBackView::OnCopyDone(WPARAM wParam, LPARAM lParam)
{
    std::unique_ptr<CDoneInfo> done(reinterpret_cast<CDoneInfo*>(lParam));
    int i = GetDocument()->FindByJob((UINT)wParam);
    if (i < 0) return 0;
    CPlanEntry& e = *Plans()[i];

    bool cancelled = done ? done->bCancelled : true;
    int  errors    = done ? done->nErrors : 0;
    CString result = cancelled ? _T("Cancelled")
                   : (errors > 0 ? _T("Finished with errors") : _T("Done"));

    e.bRunning = false;
    e.nPercent = 100;
    e.strCurrentFile.Empty();
    e.strStatus = result;
    e.strLastRun.Format(_T("Last run %s: %s.  %s"), (LPCTSTR)DateTimeNow(), (LPCTSTR)result,
                        done ? (LPCTSTR)done->strSummary : _T(""));
    if (done) e.log.push_back(done->strSummary);
    e.log.push_back(_T("Run ended ") + DateTimeNow() + _T(" (") + result + _T(")"));

    RefreshRow(i);
    SetStatusText(e.plan.strTitle + _T(": ") + result);
    if (errors > 0 && !cancelled) ::MessageBeep(MB_ICONWARNING);
    return 0;
}

LRESULT CLXFreeBackView::OnCopyError(WPARAM wParam, LPARAM lParam)
{
    std::unique_ptr<CString> pMsg(reinterpret_cast<CString*>(lParam));
    int i = GetDocument()->FindByJob((UINT)wParam);
    if (pMsg && i >= 0) Plans()[i]->log.push_back(_T("[ERR] ") + *pMsg);
    return 0;
}

LRESULT CLXFreeBackView::OnCopyLog(WPARAM wParam, LPARAM lParam)
{
    std::unique_ptr<CString> pMsg(reinterpret_cast<CString*>(lParam));
    int i = GetDocument()->FindByJob((UINT)wParam);
    if (pMsg && i >= 0) Plans()[i]->log.push_back(*pMsg);
    return 0;
}

// ── Closing ───────────────────────────────────────────────────────────────────

bool CLXFreeBackView::PrepareToClose()
{
    if (m_bClosing) return false;            // already waiting for jobs to stop
    if (!GetDocument()->AnyRunning()) return true;

    if (MessageBox(_T("Some backup plans are still running.\n\nStop them and exit?"),
                   _T("LXFreeBack"), MB_YESNO | MB_ICONQUESTION) != IDYES)
        return false;

    m_bClosing = true;
    for (auto& p : Plans())
        if (p->bRunning && p->job) p->job->Cancel();

    // Give the workers a moment to finish the file they are on.
    CWaitCursor wait;
    const ULONGLONG t0 = ::GetTickCount64();
    while (::GetTickCount64() - t0 < 15000)
    {
        bool any = false;
        for (auto& p : Plans())
            if (p->job && p->job->IsRunning()) any = true;
        if (!any) break;

        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                ::PostQuitMessage((int)msg.wParam);
                return true;
            }
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
        ::Sleep(30);
    }
    return true;     // m_bClosing stays set: late conflict prompts are auto-cancelled
}
