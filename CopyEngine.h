#pragma once
#include "pch.h"
#include "resource.h"
#include "WpdHelper.h"
#include "BrowserState.h"

#include <atomic>

// ─────────────────────────────────────────────────────────────────────────────
//  Copy engine shared by the "Universal File Copier" dialog and by backup-plan
//  runs in the LXFreeBack window.
//
//  A CCopyJob copies one or more (source → destination) tasks on a worker
//  thread and reports back to a window by PostMessage.  Every message carries
//  the job ID in wParam so one window can drive several jobs at once:
//
//    WM_COPY_PROGRESS  lParam = CProgressInfo*  (receiver deletes)
//    WM_COPY_CONFLICT  lParam = CString* rel path (receiver deletes) — the
//                      receiver must answer with CCopyJob::SetConflictResolution
//    WM_COPY_ERROR     lParam = CString* message  (receiver deletes)
//    WM_COPY_LOG       lParam = CString* message  (receiver deletes)
//    WM_COPY_DONE      lParam = CDoneInfo*        (receiver deletes)
//
//  Each job owns its own counters and cancel state, so several jobs can run
//  in parallel.
// ─────────────────────────────────────────────────────────────────────────────

// ── Endpoint descriptor ───────────────────────────────────────────────────────
// Source endpoint carries a list of selected items.
// Destination endpoint always has exactly one target folder.
struct CEndpoint
{
    bool    bIsLocal = true;

    // ── Source items (only used for the source endpoint) ─────────────────────
    std::vector<CSourceItem> items;   // the checked files/folders
    // Files/folders inside the source folders that must NOT be copied
    // (same device as the source; a folder excludes its whole subtree).
    std::vector<CSourceItem> excludes;

    // ── Destination folder (used for the destination endpoint) ───────────────
    CString strLocalPath;    // local: absolute path of destination folder
    CString strDeviceId;     // MTP: PnP device ID (local: drive root)
    CString strObjectId;     // MTP: WPD object ID of destination folder
    CString strSubPath;      // destination folder below the device / drive root

    // ── Common ───────────────────────────────────────────────────────────────
    CString strDeviceName;   // friendly name, used to find the device again
    CString strDeviceIdSrc;  // kept so worker can open the right device
    CString strDisplayPath;  // human-readable for the edit box

    bool HasSource() const { return !items.empty(); }
    bool HasDest()   const
    {
        return bIsLocal ? !strLocalPath.IsEmpty()
                        : (!strDeviceId.IsEmpty() && !strObjectId.IsEmpty());
    }
};

// Local drive root ("D:\\") + sub path → absolute path, and back.
CString JoinDevicePath(const CString& root, const CString& subPath);
CString SubPathBelow(const CString& root, const CString& fullPath);

// "Overwrite" / "Skip" / "Overwrite if newer" (or "Ask" for 0)
CString ConflictPolicyName(int nPolicy);

// One unit of work: copy every item of src into the dst folder.
struct CCopyTask
{
    CEndpoint src;
    CEndpoint dst;
    // What to do when a file already exists at the destination:
    // 0 = ask with the conflict dialog (Universal File Copier), or
    // CONFLICT_OVERWRITE / CONFLICT_SKIP / CONFLICT_NEWER (backup plans).
    int nOnConflict = 0;
};

// ── Progress payload ──────────────────────────────────────────────────────────
struct CProgressInfo
{
    int     nCurrent;        // file index within the current task (1-based)
    int     nTotal;          // file count of the current task
    CString strCurrentFile;
    int     nTask      = 1;  // current task (1-based)
    int     nTaskCount = 1;

    // Overall completion across all tasks, 0..100
    int OverallPercent() const
    {
        if (nTaskCount <= 0) return 0;
        double inTask = nTotal > 0 ? (double)nCurrent / nTotal : 0.0;
        double v = ((nTask - 1) + inTask) / nTaskCount;
        int pct = (int)(v * 100.0);
        return pct < 0 ? 0 : (pct > 100 ? 100 : pct);
    }
};

struct CDoneInfo
{
    bool    bCancelled = false;
    CString strSummary;
    int     nCopied  = 0;
    int     nSkipped = 0;
    int     nErrors  = 0;
    int     nTotal   = 0;
};

struct FlatFile {
    CString strSrcId;    // MTP object-id  OR  local absolute path
    CString strRelPath;  // relative path used at the destination
};

// ─────────────────────────────────────────────────────────────────────────────
class CCopyJob
{
public:
    // hNotify: window that receives the WM_COPY_* messages.
    // bResolveSaved: true for tasks loaded from a backup plan — MTP devices
    //   and objects are looked up again by name/path, missing local
    //   destination folders are created.
    CCopyJob(HWND hNotify, std::vector<CCopyTask> tasks, bool bResolveSaved);
    ~CCopyJob();

    CCopyJob(const CCopyJob&) = delete;
    CCopyJob& operator=(const CCopyJob&) = delete;

    UINT GetId() const     { return m_nId; }
    bool IsRunning() const { return m_bRunning.load(); }

    // Starts the worker thread.  The job keeps itself alive (through the
    // shared_ptr) until the thread ends, even if the caller drops it.
    static bool Start(const std::shared_ptr<CCopyJob>& job);

    void Cancel();
    bool IsCancelled();

    // Called by the UI thread in response to WM_COPY_CONFLICT.
    void SetConflictResolution(int nResolution, bool bApplyToAll);
    void GetConflictTimes(FILETIME& ftSrc, FILETIME& ftDst);

    // Expand a CSourceItem into a flat list of (objectId-or-localPath, relPath) pairs.
    // For a file item the list has exactly one entry with relPath == filename.
    // For a folder item the list contains every file in the subtree.
    // Anything listed in pExcludes (matched by local path / MTP object ID) is
    // left out; an excluded folder is not even walked.
    static std::vector<FlatFile> ExpandItem(const CSourceItem& item,
                                            CWpdHelper* pWpd /*nullptr for local*/,
                                            const std::vector<CSourceItem>* pExcludes = nullptr);

private:
    struct Stat {
        int  nTotal      = 0;
        int  nCopied     = 0;
        int  nSkipped    = 0;
        int  nErrors     = 0;
        int  nCurrent    = 0;
        bool bHaveGlobal = false;
        int  nGlobal     = CONFLICT_SKIP;
    };

    static UINT ThreadProc(LPVOID pv);
    void Run();

    bool PrepareTask(CCopyTask& t);
    bool ResolveDevice(CEndpoint& ep);
    bool ResolveLocal(CEndpoint& ep);

    void DoLocalToLocal(const CCopyTask& t);
    void DoLocalToMtp  (const CCopyTask& t);
    void DoMtpToLocal  (const CCopyTask& t);
    void DoMtpToMtp    (const CCopyTask& t);

    int  ResolveConflict(const CString& relPath, FILETIME ftSrc, FILETIME ftDst);
    void InitStat(int nTotal);
    void ReportProgress(const CString& relPath);
    void PostError(const CString& msg);
    void PostLog(const CString& msg);

    template <class T> void Post(UINT msg, T* payload)
    {
        if (!::IsWindow(m_hNotify) ||
            !::PostMessage(m_hNotify, msg, (WPARAM)m_nId, (LPARAM)payload))
            delete payload;
    }

    static CString EnsureMtpFolders(CWpdHelper& wpd,
        const CString& strRootObjectId,
        const CString& strRelDirPath);

    static std::atomic<UINT> s_nextId;

    UINT                   m_nId;
    HWND                   m_hNotify;
    std::vector<CCopyTask> m_tasks;
    bool                   m_bResolveSaved;
    std::atomic<bool>      m_bRunning{ false };

    int  m_nTask = 0;          // 1-based index of the running task
    int  m_nTaskPolicy = 0;    // nOnConflict of the running task
    Stat m_stats;              // per task
    int  m_nAllCopied = 0, m_nAllSkipped = 0, m_nAllErrors = 0, m_nAllTotal = 0;

    // Shared with the UI thread
    CCriticalSection m_cs;
    BOOL     m_bCancelled          = FALSE;
    BOOL     m_bConflictReady      = FALSE;
    int      m_nConflictResolution = CONFLICT_SKIP;
    FILETIME m_ftSrc = {};
    FILETIME m_ftDst = {};
};
