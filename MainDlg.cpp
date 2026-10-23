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
END_MESSAGE_MAP()

CMainDlg::CMainDlg(CWnd* pParent) : CDialogEx(IDD_MAIN_DIALOG, pParent) {}
CMainDlg::~CMainDlg() {}

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

    m_copyState.bCancelled          = FALSE;
    m_copyState.bConflictReady      = FALSE;
    m_copyState.nConflictResolution = CONFLICT_SKIP;
    m_listLog.ResetContent();
    m_progress.SetPos(0);
    m_staticStatus.SetWindowText(_T("Starting\u2026"));
    SetControlsEnabled(TRUE);

    auto* p = new TParam{ this, m_src, m_dst };
    m_pThread = AfxBeginThread(ThreadProc, p, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
    if (m_pThread) { m_pThread->m_bAutoDelete = TRUE; m_pThread->ResumeThread(); }
}

void CMainDlg::OnBtnCancelCopy()
{
    CSingleLock lk(&m_copyState.cs, TRUE);
    m_copyState.bCancelled          = TRUE;
    m_copyState.bConflictReady      = TRUE;
    m_copyState.nConflictResolution = CONFLICT_CANCEL;
    m_staticStatus.SetWindowText(_T("Cancelling\u2026"));
}

// ── UI message handlers ───────────────────────────────────────────────────────

LRESULT CMainDlg::OnCopyProgress(WPARAM, LPARAM lParam)
{
    auto* info = reinterpret_cast<CProgressInfo*>(lParam);
    if (info)
    {
        int pct = info->nTotal > 0 ? (int)(100LL * info->nCurrent / info->nTotal) : 0;
        m_progress.SetPos(pct);
        CString s;
        s.Format(_T("Copying %d of %d  (%d%%)"), info->nCurrent, info->nTotal, pct);
        m_staticStatus.SetWindowText(s);
        m_staticFile.SetWindowText(info->strCurrentFile);
        delete info;
    }
    return 0;
}

LRESULT CMainDlg::OnCopyConflict(WPARAM, LPARAM lParam)
{
    auto* pPath = reinterpret_cast<CString*>(lParam);

    CConflictInfo info;
    info.strRelPath = pPath ? *pPath : _T("(unknown)");
    delete pPath;

    // Read mod times that the worker stored before posting this message
    {
        CSingleLock lk(&m_copyState.cs, TRUE);
        info.ftSrc = m_copyState.ftSrc;
        info.ftDst = m_copyState.ftDst;
    }

    CConflictDialog dlg(info, this);
    dlg.DoModal();

    CSingleLock lk(&m_copyState.cs, TRUE);
    int raw = dlg.m_nResolution;
    if (dlg.m_bApplyToAll) raw |= 0x10000;
    m_copyState.nConflictResolution = raw;
    m_copyState.bConflictReady      = TRUE;
    return 0;
}

LRESULT CMainDlg::OnCopyDone(WPARAM wParam, LPARAM lParam)
{
    SetControlsEnabled(FALSE);
    m_progress.SetPos(100);
    m_staticStatus.SetWindowText(wParam ? _T("Cancelled.") : _T("Done!"));
    m_staticFile.SetWindowText(_T(""));
    auto* pLog = reinterpret_cast<CString*>(lParam);
    if (pLog) { AddLog(*pLog); delete pLog; }
    return 0;
}

LRESULT CMainDlg::OnCopyError(WPARAM, LPARAM lParam)
{
    auto* pMsg = reinterpret_cast<CString*>(lParam);
    if (pMsg) { AddLog(_T("[ERR] ") + *pMsg); delete pMsg; }
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

// ── ResolveConflict ───────────────────────────────────────────────────────────

int CMainDlg::ResolveConflict(CMainDlg* pDlg, CCopyState& state,
                               const CString& relPath,
                                FILETIME ftSrc, FILETIME ftDst,
                               bool& bHaveGlobal, int& nGlobal)
{
    if (bHaveGlobal) return stats.nGlobal;

    {
        CSingleLock lk(&state.cs, TRUE);
        state.bConflictReady = FALSE;
        state.ftSrc = ftSrc;
        state.ftDst = ftDst;
    }
    pDlg->PostMessage(WM_COPY_CONFLICT, 0, (LPARAM)(new CString(relPath)));

    while (true) {
        Sleep(50);
        CSingleLock lk(&state.cs, TRUE);
        if (state.bConflictReady) {
            int raw = state.nConflictResolution;
            bool all = (raw & 0x10000) != 0;
            int  res = raw & 0xFFFF;
            if (all) { stats.bHaveGlobal = true; nGlobal = res; }
            return res;
        }
    }
}

// ── EnsureMtpFolders ─────────────────────────────────────────────────────────

CString CMainDlg::EnsureMtpFolders(CWpdHelper& wpd,
                                    const CString& strRootObjectId,
                                    const CString& strRelDirPath)
{
    int flag = 0;
    CString parentId = strRootObjectId;
    fs::path dirPath(strRelDirPath.GetString());
    for (auto& part : dirPath)
    {
        CString partName(part.c_str());
        if (partName.IsEmpty() || partName == _T(".")) continue;

        CString existing = wpd.FindChild(parentId, partName);
        if (existing.IsEmpty())
        {
			CString partId = wpd.CreateFolder(parentId, partName);
            if (!partId.IsEmpty())
            {
                wpd.AddChild(parentId, partName, partId);
                parentId = partId;
            }
        }
        else
        {
			parentId = existing;
        }

        if (parentId.IsEmpty()) return _T("");
    }
    return parentId;
}

// ── ExpandItem ────────────────────────────────────────────────────────────────
// Converts a single CSourceItem into a flat list of files with relative paths.
//
// Rules:
//   • File item  → one FlatFile entry; relPath = just the filename.
//   • Folder item → recursively enumerate all files under it;
//                   relPath = itemName\sub\file.ext
//     (the item's own name is the first component so the hierarchy is preserved
//      even when multiple folders are selected.)
//
// pWpd is non-null for MTP items, nullptr for local items.

std::vector<FlatFile>
CMainDlg::ExpandItem(const CSourceItem& item, CWpdHelper* pWpd)
{
    std::vector<FlatFile> result;

    if (!item.bIsFolder)
    {
        // Single file
        FlatFile ff;
        ff.strSrcId   = item.bIsFolder ? item.strObjectId : item.strObjectId;
        ff.strSrcId   = item.strObjectId.IsEmpty() ? item.strLocalPath : item.strObjectId;
        ff.strRelPath = item.strName;
        result.push_back(ff);
        return result;
    }

    // Folder: recurse
    if (pWpd)
    {
        // MTP folder
        pWpd->EnumerateFiles(item.strObjectId, item.strName,
            [&](const MtpNode& node, const CString& rel)
            {
                FlatFile ff;
                ff.strSrcId   = node.strObjectId;
                ff.strRelPath = rel;
                result.push_back(ff);
            });
    }
    else
    {
        // Local folder
        fs::path root(item.strLocalPath.GetString());
        fs::path baseName(item.strName.GetString());
        std::error_code ec;
        for (auto& e : fs::recursive_directory_iterator(root,
                           fs::directory_options::skip_permission_denied, ec))
        {
            if (!e.is_regular_file(ec)) continue;
            fs::path rel = baseName / fs::relative(e.path(), root, ec);
            FlatFile ff;
            ff.strSrcId   = CString(e.path().c_str());
            ff.strRelPath = CString(rel.c_str());
            result.push_back(ff);
        }
    }
    return result;
}

// ── ShouldOverwrite ───────────────────────────────────────────────────────────
// Applies CONFLICT_NEWER: returns true if source is strictly newer than dest.
// If either time is unknown (zero) falls back to overwrite (safe default).
static bool ShouldOverwrite(FILETIME ftSrc, FILETIME ftDst)
{
    if (ftSrc.dwLowDateTime == 0 && ftSrc.dwHighDateTime == 0) return true;
    if (ftDst.dwLowDateTime == 0 && ftDst.dwHighDateTime == 0) return true;
    return CompareFileTime(&ftSrc, &ftDst) > 0;   // src > dst → src is newer
}

// ═════════════════════════════════════════════════════════════════════════════
//  Worker thread dispatcher
// ═════════════════════════════════════════════════════════════════════════════

UINT CMainDlg::ThreadProc(LPVOID pv)
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    auto* p = reinterpret_cast<TParam*>(pv);

    const ULONGLONG tstart = GetTickCount64();

    try
    {
        bool srcLocal = p->src.bIsLocal;
        bool dstLocal = p->dst.bIsLocal;
        if      ( srcLocal &&  dstLocal) DoLocalToLocal(p, p->pDlg->m_copyState);
        else if ( srcLocal && !dstLocal) DoLocalToMtp  (p, p->pDlg->m_copyState);
        else if (!srcLocal &&  dstLocal) DoMtpToLocal  (p, p->pDlg->m_copyState);
        else                             DoMtpToMtp    (p, p->pDlg->m_copyState);

        bool cancelled = false;
        { CSingleLock lk(&p->pDlg->m_copyState.cs, TRUE); cancelled = p->pDlg->m_copyState.bCancelled; }

        ULONGLONG elapsed = GetTickCount64() - tstart;
        CString s; s.Format(_T("Copied:%d Skipped:%d Errors:%d Total:%d  Time: %llu:%llu:%llu"),
            stats.nCopied, stats.nSkipped, stats.nErrors, stats.nTotal, 
            (elapsed / 1000 / 60 / 60), elapsed / 1000 / 60 % 60, elapsed / 1000 % 3600);
        p->pDlg->PostMessage(WM_COPY_DONE, cancelled ? 1 : 0, (LPARAM)(new CString(s)));
    }
    catch (const std::exception& ex)
    {
        auto* pErr = new CString(ex.what());
        p->pDlg->PostMessage(WM_COPY_ERROR, 0, (LPARAM)pErr);
        p->pDlg->PostMessage(WM_COPY_DONE,  1, 0);
    }
    delete p;
    CoUninitialize();
    return 0;
}

// ─── Helper: build flat file list from all source items ──────────────────────

static std::vector<FlatFile>
BuildFileList(const CEndpoint& src, CWpdHelper* pWpd)
{
    std::vector<FlatFile> all;
    for (auto& item : src.items)
    {
        auto expanded = CMainDlg::ExpandItem(item, pWpd);
        all.insert(all.end(), expanded.begin(), expanded.end());
    }
    return all;
}

void CMainDlg::InitStat(int t)
{
    stats.nTotal = t;
    stats.nCopied = 0;
    stats.nSkipped = 0;
    stats.nErrors = 0;
    stats.nCurrent = 0;
    stats.bHaveGlobal = false;
    stats.nGlobal = CONFLICT_SKIP;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Local → Local
// ─────────────────────────────────────────────────────────────────────────────
void CMainDlg::DoLocalToLocal(TParam* p, CCopyState& state)
{
    auto files   = BuildFileList(p->src, nullptr);
    InitStat((int)files.size());
    fs::path dstRoot = p->dst.strLocalPath.GetString();

    for (auto& ff : files)
    {
        { CSingleLock lk(&state.cs, TRUE); if (state.bCancelled) break; }
        ++stats.nCurrent;
        p->pDlg->PostMessage(WM_COPY_PROGRESS, 0,
            (LPARAM)(new CProgressInfo{ stats.nCurrent, stats.nTotal, ff.strRelPath }));

        fs::path dst = dstRoot / ff.strRelPath.GetString();
        if (fs::exists(dst)) {
            WIN32_FILE_ATTRIBUTE_DATA fad = {};
            FILETIME ftSrc = {}, ftDst = {};
            if (GetFileAttributesExW(ff.strSrcId.GetString(), GetFileExInfoStandard, &fad))
                ftSrc = fad.ftLastWriteTime;
            if (GetFileAttributesExW(dst.c_str(), GetFileExInfoStandard, &fad))
                ftDst = fad.ftLastWriteTime;
            int res = ResolveConflict(p->pDlg, state, ff.strRelPath, ftSrc, ftDst, stats.bHaveGlobal, stats.nGlobal);
            if (res == CONFLICT_CANCEL) { CSingleLock lk(&state.cs,TRUE); state.bCancelled=TRUE; break; }
            if (res == CONFLICT_SKIP)   { ++stats.nSkipped; continue; }
            if (res == CONFLICT_NEWER && !ShouldOverwrite(ftSrc, ftDst))
            {
                ++stats.nSkipped; continue;
            }
        }
        std::error_code ec;
        fs::create_directories(dst.parent_path(), ec);
        fs::copy_file(fs::path(ff.strSrcId.GetString()), dst,
                      fs::copy_options::overwrite_existing, ec);
        if (ec) {
            p->pDlg->PostMessage(WM_COPY_ERROR, 0,
                (LPARAM)(new CString(ff.strRelPath + _T(": ") + CString(ec.message().c_str()))));
            ++stats.nErrors;
        } else ++stats.nCopied;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Local → MTP
// ─────────────────────────────────────────────────────────────────────────────
void CMainDlg::DoLocalToMtp(TParam* p, CCopyState& state)
{
    CWpdHelper wpd;
    if (!wpd.OpenDevice(p->dst.strDeviceId))
        throw std::runtime_error("Failed to open destination MTP device.");
    wpd.RefreshContent();

    auto files   = BuildFileList(p->src, nullptr);
    InitStat((int)files.size());

    for (auto& ff : files)
    {
        { CSingleLock lk(&state.cs, TRUE); if (state.bCancelled) break; }
        ++stats.nCurrent;
        p->pDlg->PostMessage(WM_COPY_PROGRESS, 0,
            (LPARAM)(new CProgressInfo{ stats.nCurrent, stats.nTotal, ff.strRelPath }));

        fs::path rel(ff.strRelPath.GetString());
        CString  fileName(rel.filename().c_str());
        CString  parentId = EnsureMtpFolders(wpd, p->dst.strObjectId,
                                CString(rel.parent_path().c_str()));
        if (parentId.IsEmpty()) {
            p->pDlg->PostMessage(WM_COPY_ERROR, 0,
                (LPARAM)(new CString(_T("Failed to create folder for: ") + ff.strRelPath)));
            ++stats.nErrors; continue;
        }
        CString existing = wpd.FindChild(parentId, fileName);
        if (!existing.IsEmpty()) {
            WIN32_FILE_ATTRIBUTE_DATA fad = {};
            FILETIME ftSrc = {}, ftDst = {};
            if (GetFileAttributesExW(ff.strSrcId.GetString(), GetFileExInfoStandard, &fad))
                ftSrc = fad.ftLastWriteTime;
            ftDst = wpd.GetModifiedTime(existing);
            int res = ResolveConflict(p->pDlg, state, ff.strRelPath, ftSrc, ftDst, stats.bHaveGlobal, stats.nGlobal);
            if (res == CONFLICT_CANCEL) { CSingleLock lk(&state.cs,TRUE); state.bCancelled=TRUE; break; }
            if (res == CONFLICT_SKIP)   { ++stats.nSkipped; continue; }
            if (res == CONFLICT_NEWER && !ShouldOverwrite(ftSrc, ftDst))
            {
                ++stats.nSkipped; continue;
            }
            wpd.DeleteObject(existing);
        }
        CString newId = wpd.CopyToDevice(ff.strSrcId, parentId, fileName);
        if (newId.IsEmpty()) {
            p->pDlg->PostMessage(WM_COPY_ERROR, 0,
                (LPARAM)(new CString(_T("Failed to copy: ") + ff.strRelPath)));
            ++stats.nErrors;
        } else ++stats.nCopied;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  MTP → Local
// ─────────────────────────────────────────────────────────────────────────────
void CMainDlg::DoMtpToLocal(TParam* p, CCopyState& state)
{
    CWpdHelper wpd;
    if (!wpd.OpenDevice(p->src.strDeviceId))
        throw std::runtime_error("Failed to open source MTP device.");
    wpd.RefreshContent();

    auto files   = BuildFileList(p->src, &wpd);
    InitStat((int)files.size());
    fs::path dstRoot = p->dst.strLocalPath.GetString();

    for (auto& ff : files)
    {
        { CSingleLock lk(&state.cs, TRUE); if (state.bCancelled) break; }
        ++stats.nCurrent;
        p->pDlg->PostMessage(WM_COPY_PROGRESS, 0,
            (LPARAM)(new CProgressInfo{ stats.nCurrent, stats.nTotal, ff.strRelPath }));

        fs::path dst = dstRoot / ff.strRelPath.GetString();
        if (fs::exists(dst)) {
            WIN32_FILE_ATTRIBUTE_DATA fad = {};
            FILETIME ftSrc = wpd.GetModifiedTime(ff.strSrcId);
            FILETIME ftDst = {};
            if (GetFileAttributesExW(dst.c_str(), GetFileExInfoStandard, &fad))
                ftDst = fad.ftLastWriteTime;
            int res = ResolveConflict(p->pDlg, state, ff.strRelPath, ftSrc, ftDst, stats.bHaveGlobal, stats.nGlobal);
            if (res == CONFLICT_CANCEL) { CSingleLock lk(&state.cs,TRUE); state.bCancelled=TRUE; break; }
            if (res == CONFLICT_SKIP)   { ++stats.nSkipped; continue; }
            if (res == CONFLICT_NEWER && !ShouldOverwrite(ftSrc, ftDst))
            {
                ++stats.nSkipped; continue;
            }
        }
        std::error_code ec;
        fs::create_directories(dst.parent_path(), ec);
        if (!wpd.CopyFromDevice(ff.strSrcId, CString(dst.c_str()))) {
            p->pDlg->PostMessage(WM_COPY_ERROR, 0,
                (LPARAM)(new CString(_T("Failed to copy: ") + ff.strRelPath)));
            ++stats.nErrors;
        } else ++stats.nCopied;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  MTP → MTP  (via host temp file — WPD has no copy-object API)
// ─────────────────────────────────────────────────────────────────────────────
void CMainDlg::DoMtpToMtp(TParam* p, CCopyState& state)
{
    CWpdHelper wpdSrc, wpdDst;
    if (!wpdSrc.OpenDevice(p->src.strDeviceId))
        throw std::runtime_error("Failed to open source MTP device.");
    wpdSrc.RefreshContent();
    if (!wpdDst.OpenDevice(p->dst.strDeviceId))
        throw std::runtime_error("Failed to open destination MTP device.");
    wpdDst.RefreshContent();

    auto files   = BuildFileList(p->src, &wpdSrc);
    InitStat((int)files.size());

    // Temp directory on the host for buffering
    WCHAR tmpBuf[MAX_PATH];
    GetTempPathW(MAX_PATH, tmpBuf);
    fs::path tempDir = fs::path(tmpBuf) / L"FileCopierMTP_tmp";
    std::error_code ec;
    fs::create_directories(tempDir, ec);

    for (auto& ff : files)
    {
        { CSingleLock lk(&state.cs, TRUE); if (state.bCancelled) break; }
        ++stats.nCurrent;
        p->pDlg->PostMessage(WM_COPY_PROGRESS, 0,
            (LPARAM)(new CProgressInfo{ stats.nCurrent, stats.nTotal, ff.strRelPath }));

        fs::path rel(ff.strRelPath.GetString());
        CString  fileName(rel.filename().c_str());
        CString  parentId = EnsureMtpFolders(wpdDst, p->dst.strObjectId,
                                 CString(rel.parent_path().c_str()));
        if (parentId.IsEmpty()) {
            p->pDlg->PostMessage(WM_COPY_ERROR, 0,
                (LPARAM)(new CString(_T("Failed to create dst folder: ") + ff.strRelPath)));
            ++stats.nErrors; continue;
        }
        CString existing = wpdDst.FindChild(parentId, fileName);
        if (!existing.IsEmpty()) {
            FILETIME ftSrc = wpdSrc.GetModifiedTime(ff.strSrcId);
            FILETIME ftDst = wpdDst.GetModifiedTime(existing);
            int res = ResolveConflict(p->pDlg, state, ff.strRelPath, ftSrc, ftDst, stats.bHaveGlobal, stats.nGlobal);
            if (res == CONFLICT_CANCEL) { CSingleLock lk(&state.cs,TRUE); state.bCancelled=TRUE; break; }
            if (res == CONFLICT_SKIP)   { ++stats.nSkipped; continue; }
            if (res == CONFLICT_NEWER && !ShouldOverwrite(ftSrc, ftDst))
            {
                ++stats.nSkipped; continue;
            }
            wpdDst.DeleteObject(existing);
        }

        // Pull to temp file
        fs::path tmp = tempDir / rel.filename();
        CString  tmpStr(tmp.c_str());
        bool ok = wpdSrc.CopyFromDevice(ff.strSrcId, tmpStr);
        if (ok) {
            CString newId = wpdDst.CopyToDevice(tmpStr, parentId, fileName);
            ok = !newId.IsEmpty();
            fs::remove(tmp, ec);
        }
        if (!ok) {
            p->pDlg->PostMessage(WM_COPY_ERROR, 0,
                (LPARAM)(new CString(_T("Failed to copy: ") + ff.strRelPath)));
            ++stats.nErrors;
        } else ++stats.nCopied;
    }

    fs::remove_all(tempDir, ec);
}
