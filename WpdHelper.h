#pragma once
#include "pch.h"

// Forward declaration of the ToWide helper defined in WpdHelper.cpp
// (needed by the inline cache invalidation method in the class body)
static inline std::wstring _WpdToWide(const CString& s)
{
#ifdef _UNICODE
    return std::wstring(s.GetString());
#else
    int n = MultiByteToWideChar(CP_ACP, 0, s.GetString(), -1, nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_ACP, 0, s.GetString(), -1, &w[0], n);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Lightweight structs that describe nodes in the MTP device tree
// ─────────────────────────────────────────────────────────────────────────────

struct MtpNode
{
    CString strObjectId;      // WPD object ID (e.g. "o1", "o2FF")
    CString strName;          // display name
    bool    bIsFolder = true;
    ULONGLONG ullSize  = 0;
};

struct MtpDeviceInfo
{
    CString strDeviceId;      // MTP: PnP device ID string
                              // Local disk: drive root, e.g. "C:\\"
    CString strFriendlyName;  // e.g. "Pixel 8"  or  "Windows (C:)"
    bool    bIsLocalDisk = false;

    // Local-disk-only fields (empty for MTP devices)
    CString strVolumeLabel;   // e.g. "Windows"
    ULONGLONG ullTotalBytes  = 0;
    ULONGLONG ullFreeBytes   = 0;
    UINT      uDriveType     = 0;   // DRIVE_FIXED, DRIVE_REMOVABLE, etc.
};

// ─────────────────────────────────────────────────────────────────────────────
// CWpdHelper  –  thin wrapper around the WPD COM APIs
// ─────────────────────────────────────────────────────────────────────────────
class CWpdHelper
{
public:
    CWpdHelper();
    ~CWpdHelper();

    // Device enumeration — returns MTP/WPD devices followed by local drives
    static std::vector<MtpDeviceInfo> EnumerateDevices();

    // Returns only local fixed/removable drives as MtpDeviceInfo entries
    // (bIsLocalDisk == true).  Called internally by EnumerateDevices but
    // also usable standalone.
    static std::vector<MtpDeviceInfo> EnumerateLocalDisks();

    // Open / close a specific device
    bool OpenDevice(const CString& strDeviceId);
    void CloseDevice();
    bool IsOpen() const { return m_pDevice != nullptr || m_bIsLocalDisk; }

    // Re-fetch IPortableDeviceContent from the already-open device.
    // Call before a multi-file transfer in case a previous failed stream
    // left the content pointer in a bad state.
    bool RefreshContent();

    // Tree navigation
    std::vector<MtpNode> GetChildren(const CString& strParentObjectId);

    // Returns the last-modified FILETIME of an object.
// For local disk: objectId is the absolute file path.
// For MTP: objectId is the WPD object ID; uses WPD_OBJECT_DATE_MODIFIED.
// Returns a zeroed FILETIME if the time cannot be retrieved.
    FILETIME GetModifiedTime(const CString& strObjectId);

    // Check whether a display-name path exists on the device.
    // strPath uses '/' or '\\' as separator and is relative to strRootObjectId
    // (pass WPD_DEVICE_OBJECT_ID / "" to search from the device root).
    //
    // Examples:
    //   PathExists("", "Internal Storage/DCIM/Camera")  → true/false
    //   PathExists("o4", "Camera/photo.jpg")             → true/false
    //
    // For local disks the path is a normal filesystem path and this delegates
    // to std::filesystem::exists().
    bool PathExists(const CString& strRootObjectId, const CString& strPath);

    // Like PathExists but also returns the object-ID of the resolved node.
    // Returns "" if any component is not found.
    // For local disks returns strPath itself when the path exists.
    CString ResolveObjectId(const CString& strRootObjectId, const CString& strPath);

    // Get the object-ID of the direct parent of any object.
    // Returns "" for the device root ("DEVICE").
    CString GetParentObjectId(const CString& strObjectId);

    // Get just the parent folder's display name (one level up).
    // e.g. for object "Internal Storage/DCIM/Camera/photo.jpg"
    // this returns "Camera".
    CString GetParentName(const CString& strObjectId);

    // Build the full slash-separated path from the device root down to the
    // named object, iteratively (no recursion, safe for deep trees).
    // e.g.  "Internal Storage/DCIM/Camera/photo.jpg"
    // The leading slash is omitted; the root itself returns "".
    CString GetFullPath(const CString& strObjectId);

    // Get only the parent portion of the full path (everything except the
    // last component), equivalent to std::filesystem::path::parent_path().
    // e.g.  "Internal Storage/DCIM/Camera"
    CString GetParentPath(const CString& strObjectId);

    // ── File transfer ────────────────────────────────────────────────────────

    // Copy one file FROM the MTP device TO a local path
    // destFile must include the filename.
    bool CopyFromDevice(const CString& strObjectId,
                        const CString& strLocalDestFile);

    // Copy one local file TO the MTP device into folder strParentObjectId.
    // Returns new object ID on success.
    CString CopyToDevice(const CString& strLocalSrcFile,
                         const CString& strParentObjectId,
                         const CString& strFileName);

    // Check whether a child with the given name exists; returns "" if not.
    CString FindChild(const CString& strParentObjectId, const CString& strName);

    void AddChild(const CString& strParentObjectId, const CString& strObjectName, const CString& strObjectId);

    // Create a folder on the device; returns new object id.
    CString CreateFolder(const CString& strParentObjectId, const CString& strName);

    // Recursively enumerate all file nodes under a folder (depth-first)
    // Each callback receives the MtpNode and its relative path from strRootId
    void EnumerateFiles(const CString& strRootObjectId,
                        const CString& strRelPrefix,
                        std::function<void(const MtpNode&, const CString&)> callback);

    // Delete an object on the device
    bool DeleteObject(const CString& strObjectId);

private:
    CComPtr<IPortableDevice>           m_pDevice;
    CComPtr<IPortableDeviceContent>    m_pContent;
    CComPtr<IPortableDeviceProperties> m_pProps;
    CString                            m_strDeviceId;
    bool                               m_bIsLocalDisk = false;

    // ── Child name→id cache (Issue 2 fix) ────────────────────────────────────
    // Maps parentObjectId → (childName → childObjectId), populated lazily by
    // GetChildren and consumed by FindChild.  Invalidated by CreateFolder and
    // DeleteObject so it never goes stale within a session.
    // Key: parent object-id string.  Value: map of lowercase-name → object-id.
    using ChildMap = std::unordered_map<std::wstring, CString>;
    std::unordered_map<std::wstring, ChildMap> m_childCache;

    void InvalidateCacheEntry(const CString& strParentObjectId)
    {
        m_childCache.erase(_WpdToWide(strParentObjectId));
    }

    CString GetStringProperty(const CString& strObjectId,
                               const PROPERTYKEY& key);
    ULONGLONG GetUInt64Property(const CString& strObjectId,
                                 const PROPERTYKEY& key);
    GUID    GetGuidProperty(const CString& strObjectId,
                             const PROPERTYKEY& key);
};
