#pragma once
#include "pch.h"
#include "WpdHelper.h"   // for MtpDeviceInfo


// ── A single selected source item ─────────────────────────────────────────────
// Represents one file or folder the user checked in the browser.
// bIsFolder == true  → copy the entire subtree recursively
// bIsFolder == false → copy just this one file
//
// For local endpoints: strLocalPath is the absolute FS path.
// For MTP endpoints:   strObjectId is the WPD object ID;
//                      strName is the display name used to build relative paths.
struct CSourceItem
{
    bool    bIsFolder = false;

    // MTP
    CString strObjectId;
    CString strName;          // display name of this item

    // Local
    CString strLocalPath;     // absolute path, e.g. "C:\\foo\\bar" or "C:\\foo\\bar\\file.txt"

    // Path below the device / drive root, as stored in backup plans:
    //   MTP:   "Internal shared storage/DCIM/Camera"  ('/' separators)
    //   Local: "Users\Me\Pictures"                  (drive root "D:\" excluded)
    // Plans keep only the device name + this sub path; the plan runner finds
    // the device by name and rebuilds the real path / fresh object ID (WPD
    // object IDs and PnP device IDs change between connections).
    CString strSubPath;
};

// ── TreeNodeSnapshot ──────────────────────────────────────────────────────────
// One entry per visible tree node, saved before DoModal returns and restored
// in OnInitDialog on the next open.

struct TreeNodeSnapshot
{
    int         depth     = 0;
    CSourceItem item;
    bool        bExpanded = false;   // was the node expanded by the user?
    bool        bSelected = false;   // Ctrl+click selected (source mode only)
    bool        bHasDummy = false;   // true = node was never expanded (lazy)
};

// ── CBrowserState ─────────────────────────────────────────────────────────────
// Plain data — no CWnd, no HWND.  Lives in CMainDlg and passed into/out of
// CMtpBrowserDlg so state survives across DoModal() calls.

struct CBrowserState
{
    bool    bInitialized   = false;
    int     nLastDeviceIdx = 0;
    CString strLastSelDisplayPath;

    std::vector<MtpDeviceInfo>    devices;        // cached device list
    std::vector<TreeNodeSnapshot> treeSnapshot;   // cached tree contents
};
