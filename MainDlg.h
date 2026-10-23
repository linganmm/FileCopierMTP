#pragma once
#include "pch.h"
#include "resource.h"
#include "WpdHelper.h"
#include "MtpBrowserDlg.h"

#include <afxwin.h>
#include <afxdialogex.h>

// ── Shared state between UI & worker ─────────────────────────────────────────
struct CCopyState
{
    CCriticalSection cs;
    int  nConflictResolution = CONFLICT_SKIP;
    BOOL bConflictReady      = FALSE;
    BOOL bCancelled          = FALSE;
    // Mod times written by the worker before posting WM_COPY_CONFLICT,
// read by the UI to populate the conflict dialog.
    FILETIME ftSrc = {};
    FILETIME ftDst = {};
};

// ── Progress payload ──────────────────────────────────────────────────────────
struct CProgressInfo
{
    int     nCurrent;
    int     nTotal;
    CString strCurrentFile;
};

// ── Endpoint descriptor ───────────────────────────────────────────────────────
// Source endpoint carries a list of selected items.
// Destination endpoint always has exactly one target folder.
struct CEndpoint
{
    bool    bIsLocal = true;

    // ── Source items (only used for the source endpoint) ─────────────────────
    std::vector<CSourceItem> items;   // the checked files/folders

    // ── Destination folder (used for the destination endpoint) ───────────────
    CString strLocalPath;    // local: absolute path of destination folder
    CString strDeviceId;     // MTP: PnP device ID
    CString strObjectId;     // MTP: WPD object ID of destination folder

    // ── Common ───────────────────────────────────────────────────────────────
    CString strDeviceIdSrc;  // kept so worker can open the right device
    CString strDisplayPath;  // human-readable for the edit box

    bool HasSource() const { return !items.empty(); }
    bool HasDest()   const
    {
        return bIsLocal ? !strLocalPath.IsEmpty()
                        : (!strDeviceId.IsEmpty() && !strObjectId.IsEmpty());
    }
};

struct FlatFile {
    CString strSrcId;    // MTP object-id  OR  local absolute path
    CString strRelPath;  // relative path used at the destination
};

struct Stat {
    int nTotal;
    int nCopied;
    int nSkipped;
    int nErrors;
    int nCurrent;
    bool bHaveGlobal{ false };
    int nGlobal{ CONFLICT_SKIP };
};

// ─────────────────────────────────────────────────────────────────────────────
class CMainDlg : public CDialogEx
{
public:
    CMainDlg(CWnd* pParent = nullptr);
    virtual ~CMainDlg();
    enum { IDD = IDD_MAIN_DIALOG };

    // Expand a CSourceItem into a flat list of (objectId-or-localPath, relPath) pairs.
    // For a file item the list has exactly one entry with relPath == filename.
    // For a folder item the list contains every file in the subtree.
    static std::vector<FlatFile> ExpandItem(const CSourceItem& item,
        CWpdHelper* pWpd /*nullptr for local*/);

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX);

    afx_msg void OnBtnBrowseSrc();
    afx_msg void OnBtnBrowseDst();
    afx_msg void OnBtnStart();
    afx_msg void OnBtnCancelCopy();

    afx_msg LRESULT OnCopyProgress(WPARAM, LPARAM);
    afx_msg LRESULT OnCopyConflict(WPARAM, LPARAM);
    afx_msg LRESULT OnCopyDone(WPARAM, LPARAM);
    afx_msg LRESULT OnCopyError(WPARAM, LPARAM);

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

    CWinThread*   m_pThread = nullptr;
    CCopyState    m_copyState;

    std::unique_ptr<CMtpBrowserDlg> m_srcDlg;
    std::unique_ptr<CMtpBrowserDlg> m_dstDlg;
    // Stored here (not inside CMtpBrowserDlg) so it survives across DoModal
    // calls without keeping a CWnd-derived object alive between invocations.
    CBrowserState m_srcState;
    CBrowserState m_dstState;

    static inline Stat   stats;

    void SetControlsEnabled(BOOL bCopying);
    void AddLog(const CString& msg);
    void UpdateStartButton();
    CString SummariseSource() const;

    // ── Thread ────────────────────────────────────────────────────────────────
    struct TParam {
        CMainDlg* pDlg;
        CEndpoint  src;
        CEndpoint  dst;
    };
    static UINT ThreadProc(LPVOID pv);

    // Four copy paths
    static void DoLocalToLocal(TParam* p, CCopyState& state);
    static void DoLocalToMtp(TParam* p, CCopyState& state);
    static void DoMtpToLocal(TParam* p, CCopyState& state);
    static void DoMtpToMtp(TParam* p, CCopyState& state);

    // Shared helpers
    static int ResolveConflict(CMainDlg* pDlg, CCopyState& state,
        const CString& relPath,
        FILETIME ftSrc, FILETIME ftDst,
        bool& bHaveGlobal, int& nGlobal);

    static CString EnsureMtpFolders(CWpdHelper& wpd,
        const CString& strRootObjectId,
        const CString& strRelDirPath);

    static void InitStat(int t);
};
