#include "pch.h"
#include "CopyEngine.h"
#include <filesystem>
namespace fs = std::filesystem;

std::atomic<UINT> CCopyJob::s_nextId{ 1 };

CString JoinDevicePath(const CString& root, const CString& subPath)
{
    CString r = root;
    if (subPath.IsEmpty()) return r;
    if (!r.IsEmpty() && r.Right(1) != _T("\\") && r.Right(1) != _T("/")) r += _T('\\');
    CString s = subPath;
    s.Replace(_T('/'), _T('\\'));
    s.TrimLeft(_T('\\'));
    return r + s;
}

CString SubPathBelow(const CString& root, const CString& fullPath)
{
    if (root.IsEmpty() || fullPath.GetLength() < root.GetLength() ||
        fullPath.Left(root.GetLength()).CompareNoCase(root) != 0)
        return CString();
    CString s = fullPath.Mid(root.GetLength());
    s.TrimLeft(_T('\\'));
    return s;
}

CString ConflictPolicyName(int nPolicy)
{
    switch (nPolicy)
    {
    case CONFLICT_OVERWRITE: return _T("Overwrite");
    case CONFLICT_SKIP:      return _T("Skip");
    case CONFLICT_NEWER:     return _T("Overwrite if newer");
    default:                 return _T("Ask");
    }
}

CCopyJob::CCopyJob(HWND hNotify, std::vector<CCopyTask> tasks, bool bResolveSaved)
    : m_nId(s_nextId++)
    , m_hNotify(hNotify)
    , m_tasks(std::move(tasks))
    , m_bResolveSaved(bResolveSaved)
{}

CCopyJob::~CCopyJob() {}

// ── Start / cancel ────────────────────────────────────────────────────────────

bool CCopyJob::Start(const std::shared_ptr<CCopyJob>& job)
{
    if (!job || job->m_bRunning.load()) return false;

    {
        CSingleLock lk(&job->m_cs, TRUE);
        job->m_bCancelled          = FALSE;
        job->m_bConflictReady      = FALSE;
        job->m_nConflictResolution = CONFLICT_SKIP;
    }
    job->m_bRunning = true;

    // The thread owns one reference so the job outlives a caller that drops it.
    auto* keep = new std::shared_ptr<CCopyJob>(job);
    CWinThread* pThread = AfxBeginThread(ThreadProc, keep,
                                         THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
    if (!pThread)
    {
        delete keep;
        job->m_bRunning = false;
        return false;
    }
    pThread->m_bAutoDelete = TRUE;
    pThread->ResumeThread();
    return true;
}

void CCopyJob::Cancel()
{
    CSingleLock lk(&m_cs, TRUE);
    m_bCancelled          = TRUE;
    m_bConflictReady      = TRUE;           // release a pending conflict wait
    m_nConflictResolution = CONFLICT_CANCEL;
}

bool CCopyJob::IsCancelled()
{
    CSingleLock lk(&m_cs, TRUE);
    return m_bCancelled != FALSE;
}

void CCopyJob::SetConflictResolution(int nResolution, bool bApplyToAll)
{
    CSingleLock lk(&m_cs, TRUE);
    int raw = nResolution;
    if (bApplyToAll) raw |= 0x10000;
    m_nConflictResolution = raw;
    m_bConflictReady      = TRUE;
}

void CCopyJob::GetConflictTimes(FILETIME& ftSrc, FILETIME& ftDst)
{
    CSingleLock lk(&m_cs, TRUE);
    ftSrc = m_ftSrc;
    ftDst = m_ftDst;
}

// ── Messages to the UI ────────────────────────────────────────────────────────

void CCopyJob::ReportProgress(const CString& relPath)
{
    auto* info = new CProgressInfo{ m_stats.nCurrent, m_stats.nTotal, relPath };
    info->nTask      = m_nTask;
    info->nTaskCount = (int)m_tasks.size();
    Post(WM_COPY_PROGRESS, info);
}

void CCopyJob::PostError(const CString& msg) { Post(WM_COPY_ERROR, new CString(msg)); }
void CCopyJob::PostLog(const CString& msg)   { Post(WM_COPY_LOG,   new CString(msg)); }

// ── ResolveConflict ───────────────────────────────────────────────────────────

int CCopyJob::ResolveConflict(const CString& relPath, FILETIME ftSrc, FILETIME ftDst)
{
    // A fixed strategy chosen for this item → no prompt
    if (m_nTaskPolicy == CONFLICT_OVERWRITE || m_nTaskPolicy == CONFLICT_SKIP ||
        m_nTaskPolicy == CONFLICT_NEWER)
        return m_nTaskPolicy;

    if (m_stats.bHaveGlobal) return m_stats.nGlobal;

    {
        CSingleLock lk(&m_cs, TRUE);
        if (m_bCancelled) return CONFLICT_CANCEL;
        m_bConflictReady = FALSE;
        m_ftSrc = ftSrc;
        m_ftDst = ftDst;
    }
    auto* pPath = new CString(relPath);
    if (!::IsWindow(m_hNotify) ||
        !::PostMessage(m_hNotify, WM_COPY_CONFLICT, (WPARAM)m_nId, (LPARAM)pPath))
    {
        // Nobody can answer — treat as cancel rather than hanging forever.
        delete pPath;
        return CONFLICT_CANCEL;
    }

    while (true)
    {
        Sleep(50);
        CSingleLock lk(&m_cs, TRUE);
        if (m_bCancelled) return CONFLICT_CANCEL;
        if (m_bConflictReady)
        {
            int  raw = m_nConflictResolution;
            bool all = (raw & 0x10000) != 0;
            int  res = raw & 0xFFFF;
            if (all) { m_stats.bHaveGlobal = true; m_stats.nGlobal = res; }
            return res;
        }
        if (!::IsWindow(m_hNotify)) return CONFLICT_CANCEL;
    }
}

// ── EnsureMtpFolders ─────────────────────────────────────────────────────────

CString CCopyJob::EnsureMtpFolders(CWpdHelper& wpd,
                                   const CString& strRootObjectId,
                                   const CString& strRelDirPath)
{
    CString parentId = strRootObjectId;
    CString rel = strRelDirPath;
    rel.Replace(_T('/'), _T('\\'));
    fs::path dirPath(rel.GetString());
    for (auto& part : dirPath)
    {
        CString partName(part.c_str());
        if (partName.IsEmpty() || partName == _T(".") ||
            partName == _T("\\") || partName == _T("/")) continue;

        CString existing = wpd.FindChild(parentId, partName);
        if (existing.IsEmpty())
        {
            CString partId = wpd.CreateFolder(parentId, partName);
            if (!partId.IsEmpty())
            {
                wpd.AddChild(parentId, partName, partId);
                parentId = partId;
            }
            else
                return _T("");
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

// Local paths are compared case-insensitively, without a trailing backslash.
static std::wstring LocalKey(const CString& path)
{
    CString p = path;
    p.Replace(_T('/'), _T('\\'));
    while (p.GetLength() > 3 && p.Right(1) == _T("\\")) p = p.Left(p.GetLength() - 1);
    p.MakeLower();
    return std::wstring(p.GetString());
}

std::vector<FlatFile> CCopyJob::ExpandItem(const CSourceItem& item, CWpdHelper* pWpd,
                                           const std::vector<CSourceItem>* pExcludes)
{
    std::vector<FlatFile> result;

    // Build the exclusion set: MTP object IDs, or normalised local paths
    std::set<std::wstring> excl;
    if (pExcludes)
        for (auto& x : *pExcludes)
            excl.insert(pWpd ? std::wstring(x.strObjectId.GetString())
                             : LocalKey(x.strLocalPath.IsEmpty() ? x.strObjectId : x.strLocalPath));

    // The selected item itself may be excluded
    if (!excl.empty())
    {
        std::wstring self = pWpd ? std::wstring(item.strObjectId.GetString())
                                 : LocalKey(item.strLocalPath.IsEmpty() ? item.strObjectId : item.strLocalPath);
        if (excl.count(self)) return result;
    }

    if (!item.bIsFolder)
    {
        FlatFile ff;
        ff.strSrcId   = item.strObjectId.IsEmpty() ? item.strLocalPath : item.strObjectId;
        ff.strRelPath = item.strName;
        result.push_back(ff);
        return result;
    }

    if (pWpd)
    {
        // MTP folder
        std::function<bool(const MtpNode&)> skip;
        if (!excl.empty())
            skip = [&excl](const MtpNode& n) {
                return excl.count(std::wstring(n.strObjectId.GetString())) > 0;
            };
        pWpd->EnumerateFiles(item.strObjectId, item.strName,
            [&](const MtpNode& node, const CString& rel)
            {
                FlatFile ff;
                ff.strSrcId   = node.strObjectId;
                ff.strRelPath = rel;
                result.push_back(ff);
            }, skip);
    }
    else
    {
        // Local folder
        fs::path root(item.strLocalPath.GetString());
        fs::path baseName(item.strName.GetString());
        std::error_code ec;
        for (auto it = fs::recursive_directory_iterator(root,
                           fs::directory_options::skip_permission_denied, ec);
             !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
        {
            std::error_code ec2;
            if (!excl.empty() && excl.count(LocalKey(CString(it->path().c_str()))))
            {
                if (it->is_directory(ec2)) it.disable_recursion_pending();
                continue;
            }
            if (!it->is_regular_file(ec2)) continue;
            fs::path rel = baseName / fs::relative(it->path(), root, ec2);
            FlatFile ff;
            ff.strSrcId   = CString(it->path().c_str());
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

static std::vector<FlatFile> BuildFileList(const CEndpoint& src, CWpdHelper* pWpd)
{
    std::vector<FlatFile> all;
    for (auto& item : src.items)
    {
        auto expanded = CCopyJob::ExpandItem(item, pWpd, &src.excludes);
        all.insert(all.end(), expanded.begin(), expanded.end());
    }
    return all;
}

void CCopyJob::InitStat(int nTotal)
{
    bool bHaveGlobal = m_stats.bHaveGlobal;   // "apply to all" spans the whole run
    int  nGlobal     = m_stats.nGlobal;
    m_stats = Stat{};
    m_stats.nTotal      = nTotal;
    m_stats.bHaveGlobal = bHaveGlobal;
    m_stats.nGlobal     = nGlobal;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Worker thread
// ═════════════════════════════════════════════════════════════════════════════

UINT CCopyJob::ThreadProc(LPVOID pv)
{
    auto* keep = reinterpret_cast<std::shared_ptr<CCopyJob>*>(pv);
    std::shared_ptr<CCopyJob> self = *keep;
    delete keep;

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    self->Run();
    CoUninitialize();
    return 0;
}

void CCopyJob::Run()
{
    const ULONGLONG tstart = GetTickCount64();
    const int nTasks = (int)m_tasks.size();
    m_stats = Stat{};

    for (int i = 0; i < nTasks; ++i)
    {
        if (IsCancelled()) break;
        m_nTask = i + 1;
        CCopyTask t = m_tasks[i];
        m_nTaskPolicy = t.nOnConflict;

        if (nTasks > 1 || m_bResolveSaved)
        {
            CString s;
            s.Format(_T("Item %d/%d: %s  \u2192  %s"), i + 1, nTasks,
                     (LPCTSTR)t.src.strDisplayPath, (LPCTSTR)t.dst.strDisplayPath);
            PostLog(s);
        }
        if (!t.src.excludes.empty())
        {
            CString s;
            s.Format(_T("  Excluding %d file(s)/folder(s)"), (int)t.src.excludes.size());
            PostLog(s);
        }
        if (t.nOnConflict != 0)
            PostLog(_T("  If file exists: ") + ConflictPolicyName(t.nOnConflict));

        InitStat(0);
        ReportProgress(_T(""));

        try
        {
            if (m_bResolveSaved && !PrepareTask(t))
                continue;                 // reasons already logged and counted

            bool srcLocal = t.src.bIsLocal;
            bool dstLocal = t.dst.bIsLocal;
            if      ( srcLocal &&  dstLocal) DoLocalToLocal(t);
            else if ( srcLocal && !dstLocal) DoLocalToMtp  (t);
            else if (!srcLocal &&  dstLocal) DoMtpToLocal  (t);
            else                             DoMtpToMtp    (t);
        }
        catch (const std::exception& ex)
        {
            PostError(CString(ex.what()));
            ++m_stats.nErrors;
        }

        m_nAllCopied  += m_stats.nCopied;
        m_nAllSkipped += m_stats.nSkipped;
        m_nAllErrors  += m_stats.nErrors;
        m_nAllTotal   += m_stats.nTotal;
    }

    bool cancelled = IsCancelled();

    ULONGLONG sec = (GetTickCount64() - tstart) / 1000;
    auto* done = new CDoneInfo;
    done->bCancelled = cancelled;
    done->nCopied    = m_nAllCopied;
    done->nSkipped   = m_nAllSkipped;
    done->nErrors    = m_nAllErrors;
    done->nTotal     = m_nAllTotal;
    done->strSummary.Format(_T("Copied:%d Skipped:%d Errors:%d Total:%d  Time: %llu:%02llu:%02llu"),
        m_nAllCopied, m_nAllSkipped, m_nAllErrors, m_nAllTotal,
        sec / 3600, (sec / 60) % 60, sec % 60);

    m_bRunning = false;            // before DONE so the UI sees a finished job
    Post(WM_COPY_DONE, done);
}

// ── Device / path resolution for saved plans ─────────────────────────────────

// Finds the device / drive named in the plan and sets ep.strDeviceId.
// Order: exact friendly name → (local drives) the same volume label under a
// different drive letter, if unique → the device ID of an older plan file.
bool CCopyJob::ResolveDevice(CEndpoint& ep)
{
    auto devices = CWpdHelper::EnumerateDevices();

    if (!ep.strDeviceName.IsEmpty())
    {
        for (auto& d : devices)
            if (d.bIsLocalDisk == ep.bIsLocal && d.strFriendlyName == ep.strDeviceName)
            {
                ep.strDeviceId = d.strDeviceId;
                return true;
            }

        if (ep.bIsLocal)
        {
            // "Data (D:)" → label "Data"
            CString label = ep.strDeviceName;
            int paren = label.ReverseFind(_T('('));
            if (paren > 0) label = label.Left(paren);
            label.TrimRight();
            const MtpDeviceInfo* pMatch = nullptr;
            int n = 0;
            for (auto& d : devices)
                if (d.bIsLocalDisk && !label.IsEmpty() && d.strVolumeLabel == label)
                { pMatch = &d; ++n; }
            if (n == 1)
            {
                PostLog(_T("Drive \"") + ep.strDeviceName + _T("\" is now ") + pMatch->strFriendlyName);
                ep.strDeviceId = pMatch->strDeviceId;
                return true;
            }
        }
    }

    // Plan files written before device IDs were dropped
    if (!ep.strDeviceId.IsEmpty())
        for (auto& d : devices)
            if (d.bIsLocalDisk == ep.bIsLocal && d.strDeviceId.CompareNoCase(ep.strDeviceId) == 0)
                return true;

    CString name = ep.strDeviceName.IsEmpty() ? ep.strDeviceId : ep.strDeviceName;
    if (ep.bIsLocal)
        PostError(_T("Drive not found: ") + name);
    else
        PostError(_T("Device not connected: ") + name +
                  _T(" (unlock the phone and select 'File Transfer' mode)"));
    ++m_nAllErrors;
    return false;
}

// Local endpoint from a plan: find the drive, then root + sub path.
bool CCopyJob::ResolveLocal(CEndpoint& ep)
{
    if (!ResolveDevice(ep)) return false;
    const CString& root = ep.strDeviceId;
    auto fix = [&root](CSourceItem& it) {
        if (it.strSubPath.IsEmpty() && !it.strLocalPath.IsEmpty()) return;  // old plan file
        it.strLocalPath = JoinDevicePath(root, it.strSubPath);
        it.strObjectId  = it.strLocalPath;
    };
    for (auto& it : ep.items)    fix(it);
    for (auto& x  : ep.excludes) fix(x);
    if (!ep.strSubPath.IsEmpty() || ep.strLocalPath.IsEmpty())
        ep.strLocalPath = JoinDevicePath(root, ep.strSubPath);
    return true;
}

// Every failure posts one [ERR] line and adds one to the run's error count.
bool CCopyJob::PrepareTask(CCopyTask& t)
{
    int nMissing = 0;

    // ── Source ────────────────────────────────────────────────────────────────
    if (!t.src.bIsLocal)
    {
        if (!ResolveDevice(t.src)) return false;

        CWpdHelper wpd;
        if (!wpd.OpenDevice(t.src.strDeviceId))
        {
            PostError(_T("Failed to open source device: ") + t.src.strDeviceName);
            ++m_nAllErrors;
            return false;
        }
        std::vector<CSourceItem> found;
        for (auto& it : t.src.items)
        {
            if (it.strSubPath.IsEmpty()) { found.push_back(it); continue; }
            CString id = wpd.ResolveObjectId(_T(""), it.strSubPath);
            if (id.IsEmpty())
            {
                PostError(_T("Source not found on device: ") + it.strSubPath);
                ++m_nAllErrors; ++nMissing;
                continue;
            }
            CSourceItem c = it;
            c.strObjectId = id;
            found.push_back(c);
        }
        t.src.items.swap(found);

        // Exclusions are stored by path too; resolve them to today's IDs.
        std::vector<CSourceItem> excl;
        for (auto& x : t.src.excludes)
        {
            if (x.strSubPath.IsEmpty()) { excl.push_back(x); continue; }
            CString id = wpd.ResolveObjectId(_T(""), x.strSubPath);
            if (id.IsEmpty())
            {
                PostLog(_T("Exclusion not found on device (nothing to skip): ") + x.strSubPath);
                continue;
            }
            CSourceItem c = x;
            c.strObjectId = id;
            excl.push_back(c);
        }
        t.src.excludes.swap(excl);
    }
    else
    {
        if (!ResolveLocal(t.src)) return false;
        std::vector<CSourceItem> found;
        for (auto& it : t.src.items)
        {
            std::error_code ec;
            if (!fs::exists(it.strLocalPath.GetString(), ec))
            {
                PostError(_T("Source not found: ") + it.strLocalPath);
                ++m_nAllErrors; ++nMissing;
                continue;
            }
            found.push_back(it);
        }
        t.src.items.swap(found);
    }

    if (t.src.items.empty())
    {
        if (nMissing == 0)
        {
            PostError(_T("Nothing to copy for this item."));
            ++m_nAllErrors;
        }
        else
            PostLog(_T("Nothing left to copy for this item."));
        return false;
    }

    // ── Destination ───────────────────────────────────────────────────────────
    if (t.dst.bIsLocal)
    {
        if (!ResolveLocal(t.dst)) return false;
        if (t.dst.strLocalPath.IsEmpty())
        {
            PostError(_T("No destination folder set.")); ++m_nAllErrors;
            return false;
        }
        std::error_code ec;
        if (!fs::exists(t.dst.strLocalPath.GetString(), ec))
        {
            fs::create_directories(t.dst.strLocalPath.GetString(), ec);
            if (ec)
            {
                PostError(_T("Could not create destination: ") + t.dst.strLocalPath); ++m_nAllErrors;
                return false;
            }
            PostLog(_T("Created destination folder: ") + t.dst.strLocalPath);
        }
    }
    else
    {
        if (!ResolveDevice(t.dst)) return false;

        if (!t.dst.strSubPath.IsEmpty())
        {
            CWpdHelper wpd;
            if (!wpd.OpenDevice(t.dst.strDeviceId))
            {
                PostError(_T("Failed to open destination device: ") + t.dst.strDeviceName); ++m_nAllErrors;
                return false;
            }
            CString id = wpd.ResolveObjectId(_T(""), t.dst.strSubPath);
            if (id.IsEmpty())
            {
                id = EnsureMtpFolders(wpd, _T(""), t.dst.strSubPath);
                if (!id.IsEmpty())
                    PostLog(_T("Created destination folder on device: ") + t.dst.strSubPath);
            }
            if (id.IsEmpty())
            {
                PostError(_T("Destination folder not found on device: ") + t.dst.strSubPath); ++m_nAllErrors;
                return false;
            }
            t.dst.strObjectId = id;
        }
        if (t.dst.strObjectId.IsEmpty())
        {
            PostError(_T("No destination folder set.")); ++m_nAllErrors;
            return false;
        }
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Local → Local
// ─────────────────────────────────────────────────────────────────────────────
void CCopyJob::DoLocalToLocal(const CCopyTask& t)
{
    auto files = BuildFileList(t.src, nullptr);
    InitStat((int)files.size());
    fs::path dstRoot = t.dst.strLocalPath.GetString();

    for (auto& ff : files)
    {
        if (IsCancelled()) break;
        ++m_stats.nCurrent;
        ReportProgress(ff.strRelPath);

        fs::path dst = dstRoot / ff.strRelPath.GetString();
        std::error_code ec;
        if (fs::exists(dst, ec)) {
            WIN32_FILE_ATTRIBUTE_DATA fad = {};
            FILETIME ftSrc = {}, ftDst = {};
            if (GetFileAttributesExW(ff.strSrcId.GetString(), GetFileExInfoStandard, &fad))
                ftSrc = fad.ftLastWriteTime;
            if (GetFileAttributesExW(dst.c_str(), GetFileExInfoStandard, &fad))
                ftDst = fad.ftLastWriteTime;
            int res = ResolveConflict(ff.strRelPath, ftSrc, ftDst);
            if (res == CONFLICT_CANCEL) { Cancel(); break; }
            if (res == CONFLICT_SKIP)   { ++m_stats.nSkipped; continue; }
            if (res == CONFLICT_NEWER && !ShouldOverwrite(ftSrc, ftDst))
            {
                ++m_stats.nSkipped; continue;
            }
        }
        fs::create_directories(dst.parent_path(), ec);
        ec.clear();
        fs::copy_file(fs::path(ff.strSrcId.GetString()), dst,
                      fs::copy_options::overwrite_existing, ec);
        if (ec) {
            PostError(ff.strRelPath + _T(": ") + CString(ec.message().c_str()));
            ++m_stats.nErrors;
        } else ++m_stats.nCopied;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Local → MTP
// ─────────────────────────────────────────────────────────────────────────────
void CCopyJob::DoLocalToMtp(const CCopyTask& t)
{
    CWpdHelper wpd;
    if (!wpd.OpenDevice(t.dst.strDeviceId))
        throw std::runtime_error("Failed to open destination MTP device.");
    wpd.RefreshContent();

    auto files = BuildFileList(t.src, nullptr);
    InitStat((int)files.size());

    for (auto& ff : files)
    {
        if (IsCancelled()) break;
        ++m_stats.nCurrent;
        ReportProgress(ff.strRelPath);

        fs::path rel(ff.strRelPath.GetString());
        CString  fileName(rel.filename().c_str());
        CString  parentId = EnsureMtpFolders(wpd, t.dst.strObjectId,
                                CString(rel.parent_path().c_str()));
        if (parentId.IsEmpty()) {
            PostError(_T("Failed to create folder for: ") + ff.strRelPath);
            ++m_stats.nErrors; continue;
        }
        CString existing = wpd.FindChild(parentId, fileName);
        if (!existing.IsEmpty()) {
            WIN32_FILE_ATTRIBUTE_DATA fad = {};
            FILETIME ftSrc = {}, ftDst = {};
            if (GetFileAttributesExW(ff.strSrcId.GetString(), GetFileExInfoStandard, &fad))
                ftSrc = fad.ftLastWriteTime;
            ftDst = wpd.GetModifiedTime(existing);
            int res = ResolveConflict(ff.strRelPath, ftSrc, ftDst);
            if (res == CONFLICT_CANCEL) { Cancel(); break; }
            if (res == CONFLICT_SKIP)   { ++m_stats.nSkipped; continue; }
            if (res == CONFLICT_NEWER && !ShouldOverwrite(ftSrc, ftDst))
            {
                ++m_stats.nSkipped; continue;
            }
            wpd.DeleteObject(existing);
        }
        CString newId = wpd.CopyToDevice(ff.strSrcId, parentId, fileName);
        if (newId.IsEmpty()) {
            PostError(_T("Failed to copy: ") + ff.strRelPath);
            ++m_stats.nErrors;
        } else ++m_stats.nCopied;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  MTP → Local
// ─────────────────────────────────────────────────────────────────────────────
void CCopyJob::DoMtpToLocal(const CCopyTask& t)
{
    CWpdHelper wpd;
    if (!wpd.OpenDevice(t.src.strDeviceId))
        throw std::runtime_error("Failed to open source MTP device.");
    wpd.RefreshContent();

    auto files = BuildFileList(t.src, &wpd);
    InitStat((int)files.size());
    fs::path dstRoot = t.dst.strLocalPath.GetString();

    for (auto& ff : files)
    {
        if (IsCancelled()) break;
        ++m_stats.nCurrent;
        ReportProgress(ff.strRelPath);

        fs::path dst = dstRoot / ff.strRelPath.GetString();
        std::error_code ec;
        if (fs::exists(dst, ec)) {
            WIN32_FILE_ATTRIBUTE_DATA fad = {};
            FILETIME ftSrc = wpd.GetModifiedTime(ff.strSrcId);
            FILETIME ftDst = {};
            if (GetFileAttributesExW(dst.c_str(), GetFileExInfoStandard, &fad))
                ftDst = fad.ftLastWriteTime;
            int res = ResolveConflict(ff.strRelPath, ftSrc, ftDst);
            if (res == CONFLICT_CANCEL) { Cancel(); break; }
            if (res == CONFLICT_SKIP)   { ++m_stats.nSkipped; continue; }
            if (res == CONFLICT_NEWER && !ShouldOverwrite(ftSrc, ftDst))
            {
                ++m_stats.nSkipped; continue;
            }
        }
        fs::create_directories(dst.parent_path(), ec);
        if (!wpd.CopyFromDevice(ff.strSrcId, CString(dst.c_str()))) {
            PostError(_T("Failed to copy: ") + ff.strRelPath);
            ++m_stats.nErrors;
        } else ++m_stats.nCopied;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  MTP → MTP  (via host temp file — WPD has no copy-object API)
// ─────────────────────────────────────────────────────────────────────────────
void CCopyJob::DoMtpToMtp(const CCopyTask& t)
{
    CWpdHelper wpdSrc, wpdDst;
    if (!wpdSrc.OpenDevice(t.src.strDeviceId))
        throw std::runtime_error("Failed to open source MTP device.");
    wpdSrc.RefreshContent();
    if (!wpdDst.OpenDevice(t.dst.strDeviceId))
        throw std::runtime_error("Failed to open destination MTP device.");
    wpdDst.RefreshContent();

    auto files = BuildFileList(t.src, &wpdSrc);
    InitStat((int)files.size());

    // Temp directory on the host for buffering (one per job so parallel
    // plan runs don't delete each other's files)
    WCHAR tmpBuf[MAX_PATH];
    GetTempPathW(MAX_PATH, tmpBuf);
    CString tmpName;
    tmpName.Format(_T("FileCopierMTP_tmp_%u_%u"), GetCurrentProcessId(), m_nId);
    fs::path tempDir = fs::path(tmpBuf) / tmpName.GetString();
    std::error_code ec;
    fs::create_directories(tempDir, ec);

    for (auto& ff : files)
    {
        if (IsCancelled()) break;
        ++m_stats.nCurrent;
        ReportProgress(ff.strRelPath);

        fs::path rel(ff.strRelPath.GetString());
        CString  fileName(rel.filename().c_str());
        CString  parentId = EnsureMtpFolders(wpdDst, t.dst.strObjectId,
                                 CString(rel.parent_path().c_str()));
        if (parentId.IsEmpty()) {
            PostError(_T("Failed to create dst folder: ") + ff.strRelPath);
            ++m_stats.nErrors; continue;
        }
        CString existing = wpdDst.FindChild(parentId, fileName);
        if (!existing.IsEmpty()) {
            FILETIME ftSrc = wpdSrc.GetModifiedTime(ff.strSrcId);
            FILETIME ftDst = wpdDst.GetModifiedTime(existing);
            int res = ResolveConflict(ff.strRelPath, ftSrc, ftDst);
            if (res == CONFLICT_CANCEL) { Cancel(); break; }
            if (res == CONFLICT_SKIP)   { ++m_stats.nSkipped; continue; }
            if (res == CONFLICT_NEWER && !ShouldOverwrite(ftSrc, ftDst))
            {
                ++m_stats.nSkipped; continue;
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
            PostError(_T("Failed to copy: ") + ff.strRelPath);
            ++m_stats.nErrors;
        } else ++m_stats.nCopied;
    }

    fs::remove_all(tempDir, ec);
}
