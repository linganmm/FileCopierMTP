#pragma once
#include "pch.h"
#include "CopyEngine.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Backup plan model and the .lbp file format (UTF-8 JSON).
//
//  {
//    "format":   "LXFreeBack Backup Plan",
//    "version":  1,
//    "name":     "LXBackupplan1",
//    "created":  "2026-10-26T17:05:03+08:00",
//    "modified": "2026-10-26T17:20:41+08:00",
//    "items": [
//      {                                   ← one JSON object per item
//        "source": {
//          "type": "local" | "mtp",
//          "deviceId": "...", "deviceName": "...", "displayPath": "...",
//          "entries": [ { "name": "...", "isFolder": true,
//                         "path": "C:\\..."               (local)
//                         "mtpPath": "Internal shared storage/DCIM",
//                         "objectId": "o1A2"               (mtp) } ],
//          "exclusions": [ same shape as "entries" — skipped when copying ]
//        },
//        "onConflict": "overwrite" | "skip" | "overwriteIfNewer",
//        "destination": {
//          "type": "local" | "mtp",
//          "deviceId": "...", "deviceName": "...", "displayPath": "...",
//          "path": "D:\\Backup"                            (local)
//          "mtpPath": "...", "objectId": "..."             (mtp)
//        }
//      }
//    ]
//  }
// ─────────────────────────────────────────────────────────────────────────────

struct CPlanItem
{
    CEndpoint src;
    CEndpoint dst;
    // When a file already exists at the destination
    int nOnConflict = CONFLICT_NEWER;   // CONFLICT_OVERWRITE / _SKIP / _NEWER
};

struct CBackupPlan
{
    CString strTitle;
    CString strCreated;      // ISO 8601 local time with UTC offset
    CString strModified;
    std::vector<CPlanItem> items;

    CString strFilePath;     // full path of the .lbp file; empty = never saved
};

namespace PlanFile
{
    extern const TCHAR* const kExtension;     // ".lbp"
    extern const TCHAR* const kDefaultPrefix; // "LXBackupplan"

    // Folder that contains the running executable (no trailing backslash).
    CString GetExeDir();

    // Directory part of a full path (no trailing backslash).
    CString DirOf(const CString& path);

    // Current local time as "2026-10-26T17:05:03+08:00".
    CString NowIso8601();

    // "2026-10-26T17:05:03+08:00" → "2026-10-26 17:05"
    CString FormatForDisplay(const CString& iso);

    // "LXBackupplanN" with the smallest N ≥ 1 such that "<title>.lbp" exists in
    // none of the given directories.
    CString MakeDefaultTitle(const std::vector<CString>& dirs);

    // Checks that a title can be used as a file name.  Returns false and a
    // user-facing message otherwise.
    bool ValidateTitle(const CString& title, CString& err);

    // dir + "\" + title + ".lbp"
    CString BuildPath(const CString& dir, const CString& title);

    bool Save(const CBackupPlan& plan, const CString& path, CString& err);
    bool Load(const CString& path, CBackupPlan& plan, CString& err);

    // One-line, human-readable endpoint description: "[Local] C:\Photos (+2 more)"
    CString DescribeEndpoint(const CEndpoint& ep);

    std::vector<CCopyTask> ToTasks(const CBackupPlan& plan);

    // System message for a Win32 error code.
    CString ErrorText(DWORD code);
}
