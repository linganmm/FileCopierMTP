#include "pch.h"
#include "WpdHelper.h"
#include <strsafe.h>

// WPD_CONTENT_TYPE_FOLDER  {27E2E392-A111-48E0-AB0C-E17705A05F85}
const GUID WPD_CONTENT_TYPE_FOLDER_GUID =
{ 0x27E2E392, 0xA111, 0x48E0, { 0xAB, 0x0C, 0xE1, 0x77, 0x05, 0xA0, 0x5F, 0x85 } };

// WPD_CONTENT_TYPE_FUNCTIONAL_OBJECT  {99ED0160-17FF-4C44-9D98-1D7A6F941921}
const GUID WPD_CONTENT_TYPE_FUNCTIONAL_OBJECT_GUID =
{ 0x99ED0160, 0x17FF, 0x4C44, { 0x9D, 0x98, 0x1D, 0x7A, 0x6F, 0x94, 0x19, 0x21 } };

// WPD_CONTENT_TYPE_STORAGE  {23F05BBC-15DE-4C2A-A55B-A9AF5CE412EF}
const GUID WPD_CONTENT_TYPE_STORAGE_GUID =
{ 0x23F05BBC, 0x15DE, 0x4C2A, { 0xA5, 0x5B, 0xA9, 0xAF, 0x5C, 0xE4, 0x12, 0xEF } };

// WPD_CONTENT_TYPE_GENERIC_FILE  {0085E0A6-8D34-45D7-BC5C-447E59C73D48}
const GUID WPD_CONTENT_TYPE_GENERIC_FILE_GUID =
{ 0x0085E0A6, 0x8D34, 0x45D7, { 0xBC, 0x5C, 0x44, 0x7E, 0x59, 0xC7, 0x3D, 0x48 } };

// WPD_CONTENT_TYPE_ALL  {80E170D2-1055-4A3E-B952-82CC4F8A8689}
const GUID WPD_CONTENT_TYPE_ALL_GUID =
{ 0x80E170D2, 0x1055, 0x4A3E, { 0xB9, 0x52, 0x82, 0xCC, 0x4F, 0x8A, 0x86, 0x89 } };

// ── Object formats ────────────────────────────────────────────────────────────

// WPD_OBJECT_FORMAT_UNSPECIFIED  {30010000-AE6C-4804-98BA-C57B46965FE7}
const GUID WPD_OBJECT_FORMAT_UNSPECIFIED_GUID =
{ 0x30010000, 0xAE6C, 0x4804, { 0x98, 0xBA, 0xC5, 0x7B, 0x46, 0x96, 0x5F, 0xE7 } };

// WPD_OBJECT_FORMAT_PROPERTIES_ONLY  {30000000-AE6C-4804-98BA-C57B46965FE7}
const GUID WPD_OBJECT_FORMAT_PROPERTIES_ONLY_GUID =
{ 0x30000000, 0xAE6C, 0x4804, { 0x98, 0xBA, 0xC5, 0x7B, 0x46, 0x96, 0x5F, 0xE7 } };

// WPD_OBJECT_FORMAT_JFIF (JPEG)  {3010000A-AE6C-4804-98BA-C57B46965FE7}
const GUID WPD_OBJECT_FORMAT_JFIF_GUID =
{ 0x3010000A, 0xAE6C, 0x4804, { 0x98, 0xBA, 0xC5, 0x7B, 0x46, 0x96, 0x5F, 0xE7 } };

// WPD_OBJECT_FORMAT_PNG  {3010000C-AE6C-4804-98BA-C57B46965FE7}
const GUID WPD_OBJECT_FORMAT_PNG_GUID =
{ 0x3010000C, 0xAE6C, 0x4804, { 0x98, 0xBA, 0xC5, 0x7B, 0x46, 0x96, 0x5F, 0xE7 } };

// WPD_OBJECT_FORMAT_MP3  {30090000-AE6C-4804-98BA-C57B46965FE7}
const GUID WPD_OBJECT_FORMAT_MP3_GUID =
{ 0x30090000, 0xAE6C, 0x4804, { 0x98, 0xBA, 0xC5, 0x7B, 0x46, 0x96, 0x5F, 0xE7 } };

// WPD_OBJECT_FORMAT_MPEG (MP4)  {300B0000-AE6C-4804-98BA-C57B46965FE7}
const GUID WPD_OBJECT_FORMAT_MPEG_GUID =
{ 0x300B0000, 0xAE6C, 0x4804, { 0x98, 0xBA, 0xC5, 0x7B, 0x46, 0x96, 0x5F, 0xE7 } };

// WPD_OBJECT_FORMAT_TEXT  {30030000-AE6C-4804-98BA-C57B46965FE7}
const GUID WPD_OBJECT_FORMAT_TEXT_GUID =
{ 0x30030000, 0xAE6C, 0x4804, { 0x98, 0xBA, 0xC5, 0x7B, 0x46, 0x96, 0x5F, 0xE7 } };

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
// Root object-ID string.  WPD_DEVICE_OBJECT_ID is defined in the SDK as the
// wide-string literal L"DEVICE".  We always pass wide strings to WPD APIs
// directly (never through CString operator LPCTSTR) to be safe in both
// Unicode and MBCS configurations.
// ─────────────────────────────────────────────────────────────────────────────
static const WCHAR* const kDeviceRootId = L"DEVICE";

// Convert a CString to a stable std::wstring for WPD API calls.
// This avoids any ambiguity from CString::operator LPCTSTR in MBCS builds.
static std::wstring ToWide(const CString& s)
{
#ifdef _UNICODE
    return std::wstring(s.GetString());
#else
    int len = MultiByteToWideChar(CP_ACP, 0, s.GetString(), -1, nullptr, 0);
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_ACP, 0, s.GetString(), -1, &w[0], len);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
#endif
}

// ─────────────────────────────────────────────────────────────────────────────

CWpdHelper::CWpdHelper()  {}
CWpdHelper::~CWpdHelper() { CloseDevice(); }

// ── EnumerateLocalDisks ───────────────────────────────────────────────────────
// Returns one MtpDeviceInfo per logical drive (fixed + removable).
// strDeviceId is the drive root ("C:\\"), bIsLocalDisk = true.

std::vector<MtpDeviceInfo> CWpdHelper::EnumerateLocalDisks()
{
    std::vector<MtpDeviceInfo> result;

    // GetLogicalDriveStrings fills a double-null-terminated list of
    // "C:\\\0D:\\\0\0" style strings.
    DWORD needed = GetLogicalDriveStringsW(0, nullptr);
    if (needed == 0) return result;

    std::vector<WCHAR> buf(needed + 1, L'\0');
    GetLogicalDriveStringsW(needed, buf.data());

    for (const WCHAR* drv = buf.data(); *drv; drv += wcslen(drv) + 1)
    {
        UINT driveType = GetDriveTypeW(drv);

        // Skip network, RAM-disk, and unknown types; keep fixed + removable
        if (driveType != DRIVE_FIXED    &&
            driveType != DRIVE_REMOVABLE &&
            driveType != DRIVE_CDROM)
            continue;

        MtpDeviceInfo info;
        info.bIsLocalDisk = true;
        info.strDeviceId  = drv;          // e.g. "C:\\"
        info.uDriveType   = driveType;

        // Volume label
        WCHAR label[MAX_PATH + 1]  = {};
        WCHAR fsName[MAX_PATH + 1] = {};
        GetVolumeInformationW(drv, label, MAX_PATH, nullptr, nullptr, nullptr,
                              fsName, MAX_PATH);
        info.strVolumeLabel = label;

        // Disk space
        ULARGE_INTEGER freeBytesAvail = {}, totalBytes = {}, totalFree = {};
        if (GetDiskFreeSpaceExW(drv, &freeBytesAvail, &totalBytes, &totalFree))
        {
            info.ullTotalBytes = totalBytes.QuadPart;
            info.ullFreeBytes  = totalFree.QuadPart;
        }

        // Build a friendly name like "Windows (C:)"  or  "USB Drive (E:)"
        // Drive letter portion: "C:" from "C:\\"
        CString driveLetter(drv);
        driveLetter.TrimRight(_T('\\'));   // "C:"

        CString typePfx;
        if      (driveType == DRIVE_REMOVABLE) typePfx = _T("Removable");
        else if (driveType == DRIVE_CDROM)     typePfx = _T("CD/DVD");
        else                                   typePfx = _T("Local Disk");

        if (!info.strVolumeLabel.IsEmpty())
            info.strFriendlyName.Format(_T("%s (%s)"),
                                        (LPCTSTR)info.strVolumeLabel,
                                        (LPCTSTR)driveLetter);
        else
            info.strFriendlyName.Format(_T("%s (%s)"),
                                        (LPCTSTR)typePfx,
                                        (LPCTSTR)driveLetter);

        result.push_back(std::move(info));
    }
    return result;
}

// ── EnumerateDevices ─────────────────────────────────────────────────────────
// Returns MTP/WPD devices first, then local drives appended at the end.

std::vector<MtpDeviceInfo> CWpdHelper::EnumerateDevices()
{
    std::vector<MtpDeviceInfo> result;

    CComPtr<IPortableDeviceManager> pMgr;
    HRESULT hr = CoCreateInstance(CLSID_PortableDeviceManager, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pMgr));
    if (FAILED(hr) || !pMgr) return result;

    // Collect volume GUIDs that are already represented by an MTP device
    // so we can suppress them from the local-disk list.
    std::set<std::wstring> mtpVolumes;

    DWORD dwCount = 0;
    hr = pMgr->GetDevices(nullptr, &dwCount);
    if (FAILED(hr)) return result;

    pMgr->GetDevices(nullptr, &dwCount);
    if (dwCount > 0)
    {
        std::vector<LPWSTR> ids(dwCount, nullptr);
        hr = pMgr->GetDevices(ids.data(), &dwCount);
        if (SUCCEEDED(hr))
        {
            for (DWORD i = 0; i < dwCount; ++i)
            {
                if (!ids[i]) continue;

                MtpDeviceInfo info;
                info.strDeviceId = ids[i];

                DWORD cch = 0;
                pMgr->GetDeviceFriendlyName(ids[i], nullptr, &cch);
                if (cch > 0)
                {
                    std::vector<WCHAR> buf(cch + 1, L'\0');
                    pMgr->GetDeviceFriendlyName(ids[i], buf.data(), &cch);
                    info.strFriendlyName = buf.data();
                }
                else
                {
                    info.strFriendlyName = ids[i];
                }
                result.push_back(std::move(info));

                // Record the volume GUID (if any) this WPD device maps to,
                // so we can exclude it from the local-drive list
                mtpVolumes.insert(ToWide(info.strFriendlyName));

                CoTaskMemFree(ids[i]);
                ids[i] = nullptr;
            }
        }
    }

    // Append local drives, skipping any that are already in the MTP list
    auto localDisks = EnumerateLocalDisks();
    int i = 0;
    for (auto& disk : localDisks)
    {
        // Get this drive's volume GUID
        std::wstring driveRoot = ToWide(disk.strDeviceId);
        WCHAR guidPath[64] = {};

        if (disk.strVolumeLabel.IsEmpty() ||
            mtpVolumes.count(ToWide(disk.strVolumeLabel)) == 0)
            result.insert(result.begin() + (i++), std::move(disk));
    }

    return result;
}

// ── OpenDevice ───────────────────────────────────────────────────────────────
//
// KEY FIX: Use CLSID_PortableDeviceFTM (free-threaded marshaller variant).
// The standard CLSID_PortableDevice is STA-affine — if your IPortableDevice
// is created on the UI thread and then called from the worker thread, COM
// silently marshals every call back to the UI thread, which causes EnumObjects
// to time out and return empty results.  CLSID_PortableDeviceFTM avoids this.

bool CWpdHelper::OpenDevice(const CString& strDeviceId)
{
    CloseDevice();

    // ── Local disk — no WPD needed ────────────────────────────────────────────
    // A local disk device-ID is just its drive root, e.g. "C:\\".
    // We recognise it by the trailing backslash after a single drive letter.
    {
        std::wstring idW = ToWide(strDeviceId);
        if (idW.size() >= 2 && idW[1] == L':')
        {
            // Validate the drive actually exists
            UINT t = GetDriveTypeW(idW.c_str());
            if (t == DRIVE_UNKNOWN || t == DRIVE_NO_ROOT_DIR) return false;
            m_strDeviceId  = strDeviceId;
            m_bIsLocalDisk = true;
            return true;
        }
    }

    CComPtr<IPortableDeviceValues> pClientInfo;
    HRESULT hr = CoCreateInstance(CLSID_PortableDeviceValues, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pClientInfo));
    if (FAILED(hr)) return false;

    pClientInfo->SetStringValue         (WPD_CLIENT_NAME,          L"FileCopierMTP");
    pClientInfo->SetUnsignedIntegerValue(WPD_CLIENT_MAJOR_VERSION,  1);
    pClientInfo->SetUnsignedIntegerValue(WPD_CLIENT_MINOR_VERSION,  0);
    pClientInfo->SetUnsignedIntegerValue(WPD_CLIENT_REVISION,       0);

    // Prefer the FTM CoClass so the COM object is safe to call from any thread
    CComPtr<IPortableDevice> pDev;
    hr = CoCreateInstance(CLSID_PortableDeviceFTM, nullptr,
                          CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDev));
    if (FAILED(hr))
    {
        hr = CoCreateInstance(CLSID_PortableDevice, nullptr,
                              CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDev));
        if (FAILED(hr)) return false;
    }

    // Use the raw wide string — never CString::operator LPCTSTR for device IDs
    hr = pDev->Open(ToWide(strDeviceId).c_str(), pClientInfo);
    if (hr == E_ACCESSDENIED)
    {
        // Device may already be open by another client; try read-only
        pClientInfo->SetUnsignedIntegerValue(WPD_CLIENT_DESIRED_ACCESS, GENERIC_READ);
        hr = pDev->Open(ToWide(strDeviceId).c_str(), pClientInfo);
    }
    if (FAILED(hr)) return false;

    CComPtr<IPortableDeviceContent> pContent;
    hr = pDev->Content(&pContent);
    if (FAILED(hr)) return false;

    CComPtr<IPortableDeviceProperties> pProps;
    hr = pContent->Properties(&pProps);
    if (FAILED(hr)) return false;

    m_pDevice     = pDev;
    m_pContent    = pContent;
    m_pProps      = pProps;
    m_strDeviceId = strDeviceId;
    return true;
}

void CWpdHelper::CloseDevice()
{
    m_pProps.Release();
    m_pContent.Release();
    if (m_pDevice) { m_pDevice->Close(); m_pDevice.Release(); }
    m_strDeviceId.Empty();
    m_bIsLocalDisk = false;
}

// Re-fetch IPortableDeviceContent from the already-open device.
// Call this before a transfer loop in case a previous failed stream left
// the content pointer in an error state.
bool CWpdHelper::RefreshContent()
{
    if (!m_pDevice) return false;
    m_pProps.Release();
    m_pContent.Release();

    CComPtr<IPortableDeviceContent> pContent;
    if (FAILED(m_pDevice->Content(&pContent))) return false;

    CComPtr<IPortableDeviceProperties> pProps;
    if (FAILED(pContent->Properties(&pProps))) return false;

    m_pContent = pContent;
    m_pProps   = pProps;
    return true;
}

// ── Internal property helpers ─────────────────────────────────────────────────
// Each helper fetches ONE property at a time via a fresh key collection.
// Batching into one GetValues call can silently drop values on some Android
// MTP stacks, so we deliberately fetch properties individually.

CString CWpdHelper::GetStringProperty(const CString& strObjectId, const PROPERTYKEY& key)
{
    if (!m_pProps) return _T("");

    CComPtr<IPortableDeviceKeyCollection> pKeys;
    if (FAILED(CoCreateInstance(CLSID_PortableDeviceKeyCollection, nullptr,
                                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pKeys))))
        return _T("");
    pKeys->Add(key);

    std::wstring idW = ToWide(strObjectId);
    CComPtr<IPortableDeviceValues> pVals;
    if (FAILED(m_pProps->GetValues(idW.c_str(), pKeys, &pVals)))
        return _T("");

    LPWSTR pwsz = nullptr;
    CString s;
    if (SUCCEEDED(pVals->GetStringValue(key, &pwsz)) && pwsz)
    {
        s = pwsz;
        CoTaskMemFree(pwsz);
    }
    return s;
}

ULONGLONG CWpdHelper::GetUInt64Property(const CString& strObjectId, const PROPERTYKEY& key)
{
    if (!m_pProps) return 0;

    CComPtr<IPortableDeviceKeyCollection> pKeys;
    if (FAILED(CoCreateInstance(CLSID_PortableDeviceKeyCollection, nullptr,
                                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pKeys))))
        return 0;
    pKeys->Add(key);

    std::wstring idW = ToWide(strObjectId);
    CComPtr<IPortableDeviceValues> pVals;
    if (FAILED(m_pProps->GetValues(idW.c_str(), pKeys, &pVals))) return 0;

    ULONGLONG v = 0;
    pVals->GetUnsignedLargeIntegerValue(key, &v);
    return v;
}

GUID CWpdHelper::GetGuidProperty(const CString& strObjectId, const PROPERTYKEY& key)
{
    if (!m_pProps) return GUID_NULL;

    CComPtr<IPortableDeviceKeyCollection> pKeys;
    if (FAILED(CoCreateInstance(CLSID_PortableDeviceKeyCollection, nullptr,
                                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pKeys))))
        return GUID_NULL;
    pKeys->Add(key);

    std::wstring idW = ToWide(strObjectId);
    CComPtr<IPortableDeviceValues> pVals;
    if (FAILED(m_pProps->GetValues(idW.c_str(), pKeys, &pVals))) return GUID_NULL;

    GUID g = GUID_NULL;
    pVals->GetGuidValue(key, &g);
    return g;
}

// ── ResolveObjectId ───────────────────────────────────────────────────────────
// Walks strPath component-by-component from strRootObjectId using FindChild.
// Returns the object-ID of the final component, or "" if any step fails.
//
// Path rules:
//   - Separator: '/' or '\\'  (both accepted, mixed is fine)
//   - Leading separator is ignored  ("/DCIM" == "DCIM")
//   - Empty components are skipped  ("DCIM//Camera" == "DCIM/Camera")
//   - strRootObjectId == "" is treated as the device root ("DEVICE")
//
// For a local disk the strPath is a plain filesystem path; we just check
// whether it exists and return it as its own "object ID".

CString CWpdHelper::ResolveObjectId(const CString& strRootObjectId,
                                     const CString& strPath)
{
    // ── Local disk ────────────────────────────────────────────────────────────
    if (m_bIsLocalDisk)
    {
        // Build the full path: if strPath is already absolute keep it, otherwise
        // join with the drive root.
        fs::path p(strPath.GetString());
        if (p.is_relative())
        {
            fs::path root = strRootObjectId.IsEmpty()
                            ? fs::path(m_strDeviceId.GetString())
                            : fs::path(strRootObjectId.GetString());
            p = root / p;
        }
        std::error_code ec;
        return fs::exists(p, ec) ? CString(p.c_str()) : CString(_T(""));
    }

    // ── MTP device ────────────────────────────────────────────────────────────
    if (!m_pContent) return _T("");

    // Split strPath on '/' and '\\'
    // Use a simple manual split so we have no dependency on extra headers.
    CString path = strPath;
    path.Replace(_T('\\'), _T('/'));     // normalise to '/'

    CString currentId = strRootObjectId.IsEmpty()
                        ? CString(WPD_DEVICE_OBJECT_ID)
                        : strRootObjectId;

    int pos = 0;
    CString token;
    while (pos <= path.GetLength())
    {
        int sep = path.Find(_T('/'), pos);
        if (sep == -1) sep = path.GetLength();   // last component

        token = path.Mid(pos, sep - pos);
        pos   = sep + 1;

        if (token.IsEmpty()) continue;   // skip leading '/' or doubled '//'

        CString childId = FindChild(currentId, token);
        if (childId.IsEmpty())
            return _T("");   // component not found → path does not exist

        currentId = childId;
    }

    // If the original path was empty we just return the root id as-is
    return currentId;
}

// ── PathExists ────────────────────────────────────────────────────────────────
// Convenience wrapper: returns true if ResolveObjectId finds every component.

bool CWpdHelper::PathExists(const CString& strRootObjectId, const CString& strPath)
{
    return !ResolveObjectId(strRootObjectId, strPath).IsEmpty();
}

// ── GetChildren ──────────────────────────────────────────────────────────────
//
// Fixes applied:
//
//  1. LPCWSTR from std::wstring — never CString::operator LPCTSTR — passed to
//     EnumObjects so the pointer is always to aligned, null-terminated UTF-16.
//
//  2. Small batch size (4): some Android MTP stacks silently truncate or
//     error on large Next() batch sizes.
//
//  3. Both S_OK and S_FALSE from Next() are handled correctly:
//       S_OK  → full batch returned, continue.
//       S_FALSE → last (possibly partial) batch, exit after processing.
//       FAILED → abort.
//
//  4. CoTaskMemFree called for every id, with null-guard, including on the
//     error path to prevent leaks.
//
//  5. isFolder fallback: if WPD_OBJECT_CONTENT_TYPE comes back as GUID_NULL
//     (some phones don't provide it), we probe the object by attempting
//     EnumObjects on it — success means it is a container.

std::vector<MtpNode> CWpdHelper::GetChildren(const CString& strParentObjectId)
{
    std::vector<MtpNode> nodes;

    // ── Local disk path ───────────────────────────────────────────────────────
    // For a local disk, strParentObjectId is a filesystem path like "C:\\" or
    // "C:\\Users\\Alice".  We enumerate it with std::filesystem.
    if (m_bIsLocalDisk)
    {
        fs::path dir(strParentObjectId.IsEmpty()
                     ? m_strDeviceId.GetString()
                     : strParentObjectId.GetString());
        std::error_code ec;
        for (auto& entry : fs::directory_iterator(dir,
                           fs::directory_options::skip_permission_denied, ec))
        {
            MtpNode node;
            node.strName     = CString(entry.path().filename().c_str());
            node.strObjectId = CString(entry.path().c_str());  // full path as ID
            node.bIsFolder   = entry.is_directory(ec);
            if (!node.bIsFolder)
                node.ullSize = entry.is_regular_file(ec)
                               ? (ULONGLONG)entry.file_size(ec) : 0;
            nodes.push_back(std::move(node));
        }
        return nodes;
    }

    // ── MTP / WPD path ────────────────────────────────────────────────────────
    if (!m_pContent) return nodes;

    std::wstring parentIdW = strParentObjectId.IsEmpty()
                              ? std::wstring(kDeviceRootId)
                              : ToWide(strParentObjectId);

    CComPtr<IEnumPortableDeviceObjectIDs> pEnum;
    HRESULT hr = m_pContent->EnumObjects(0,                  // dwFlags must be 0
                                          parentIdW.c_str(),
                                          nullptr,            // no filter
                                          &pEnum);
    if (FAILED(hr) || !pEnum) return nodes;

    static const DWORD kBatch = 4;
    LPWSTR ids[kBatch];

    for (;;)
    {
        // Zero all slots before each call
        for (auto& p : ids) p = nullptr;
        DWORD fetched = 0;

        hr = pEnum->Next(kBatch, ids, &fetched);
        // S_FALSE = no more items (fetched may still be > 0 for the last batch)
        bool last = (hr == S_FALSE);
        if (FAILED(hr)) break;

        for (DWORD i = 0; i < fetched; ++i)
        {
            if (!ids[i]) continue;

            MtpNode node;
            node.strObjectId = ids[i];

            // ── Name ─────────────────────────────────────────────────────────
            node.strName = GetStringProperty(node.strObjectId,
                                             WPD_OBJECT_ORIGINAL_FILE_NAME);
            if (node.strName.IsEmpty())
                node.strName = GetStringProperty(node.strObjectId, WPD_OBJECT_NAME);
            if (node.strName.IsEmpty())
                node.strName = node.strObjectId;

            // ── Folder detection ──────────────────────────────────────────────
            GUID ct = GetGuidProperty(node.strObjectId, WPD_OBJECT_CONTENT_TYPE);
            if (ct != GUID_NULL)
            {
                node.bIsFolder = (ct == WPD_CONTENT_TYPE_FOLDER_GUID           ||
                                  ct == WPD_CONTENT_TYPE_FUNCTIONAL_OBJECT_GUID ||
                                  ct == WPD_CONTENT_TYPE_STORAGE_GUID);
            }
            else
            {
                // Fallback: probe the object — if EnumObjects succeeds it's a container
                CComPtr<IEnumPortableDeviceObjectIDs> pProbe;
                HRESULT hrProbe = m_pContent->EnumObjects(0, ids[i], nullptr, &pProbe);
                node.bIsFolder  = SUCCEEDED(hrProbe) && (pProbe != nullptr);
            }

            if (!node.bIsFolder)
                node.ullSize = GetUInt64Property(node.strObjectId, WPD_OBJECT_SIZE);

            nodes.push_back(std::move(node));

            CoTaskMemFree(ids[i]);
            ids[i] = nullptr;
        }

        if (last) break;
    }

    // Leak guard for any slots not freed above (error-path exit)
    for (auto& p : ids)
        if (p) { CoTaskMemFree(p); p = nullptr; }

    std::sort(nodes.begin(), nodes.end(),
        [](const MtpNode& a, const MtpNode& b) {
            return a.strName < b.strName;
        });

    return nodes;
}

// ── GetModifiedTime ───────────────────────────────────────────────────────────
// Returns the last-write FILETIME of a local file or an MTP object.
// WPD stores modification time as WPD_OBJECT_DATE_MODIFIED (a PROPVARIANT
// with vt == VT_DATE, i.e. an OLE Automation DATE / double).

FILETIME CWpdHelper::GetModifiedTime(const CString& strObjectId)
{
    FILETIME ft = {};

    if (m_bIsLocalDisk)
    {
        // Local file: use GetFileAttributesEx for a quick stat
        WIN32_FILE_ATTRIBUTE_DATA fad = {};
        if (GetFileAttributesExW(ToWide(strObjectId).c_str(),
            GetFileExInfoStandard, &fad))
            ft = fad.ftLastWriteTime;
        return ft;
    }

    // MTP: read WPD_OBJECT_DATE_MODIFIED (stored as VT_DATE)
    if (!m_pProps) return ft;

    CComPtr<IPortableDeviceKeyCollection> pKeys;
    if (FAILED(CoCreateInstance(CLSID_PortableDeviceKeyCollection, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pKeys))))
        return ft;
    pKeys->Add(WPD_OBJECT_DATE_MODIFIED);

    CComPtr<IPortableDeviceValues> pVals;
    if (FAILED(m_pProps->GetValues(ToWide(strObjectId).c_str(), pKeys, &pVals)))
        return ft;

    // WPD_OBJECT_DATE_MODIFIED is a VT_DATE (OLE Automation DATE = double)
    PROPVARIANT pv = {};
    PropVariantInit(&pv);
    if (SUCCEEDED(pVals->GetValue(WPD_OBJECT_DATE_MODIFIED, &pv)))
    {
        if (pv.vt == VT_DATE)
        {
            // Convert OLE DATE to SYSTEMTIME then to FILETIME
            SYSTEMTIME st = {};
            if (VariantTimeToSystemTime(pv.date, &st))
                SystemTimeToFileTime(&st, &ft);
        }
        PropVariantClear(&pv);
    }
    return ft;
}

// ── GetParentObjectId ─────────────────────────────────────────────────────────
// Returns the WPD object-ID of the direct parent.
// Returns "" if the object IS the device root ("DEVICE").

CString CWpdHelper::GetParentObjectId(const CString& strObjectId)
{
    if (ToWide(strObjectId) == kDeviceRootId)
        return _T("");
    return GetStringProperty(strObjectId, WPD_OBJECT_PARENT_ID);
}

// ── GetParentName ─────────────────────────────────────────────────────────────
// Returns the display name of the direct parent folder.
// e.g. for an object whose full path is "Internal Storage/DCIM/Camera/photo.jpg"
//      this returns "Camera".
// Returns "" if the parent is the device root.

CString CWpdHelper::GetParentName(const CString& strObjectId)
{
    CString parentId = GetParentObjectId(strObjectId);
    if (parentId.IsEmpty() || ToWide(parentId) == kDeviceRootId)
        return _T("");

    CString name = GetStringProperty(parentId, WPD_OBJECT_ORIGINAL_FILE_NAME);
    if (name.IsEmpty())
        name = GetStringProperty(parentId, WPD_OBJECT_NAME);
    return name;
}

// ── GetFullPath (iterative) ───────────────────────────────────────────────────
// Walks WPD_OBJECT_PARENT_ID upward from strObjectId to the device root,
// collecting each object's display name, then assembles them into a
// forward-slash-separated path string.
//
// Example result:  "Internal Storage/DCIM/Camera/photo.jpg"
//
// Iterative instead of recursive so it is safe for arbitrarily deep trees
// and produces no stack-overflow risk.

CString CWpdHelper::GetFullPath(const CString& strObjectId)
{
    if (ToWide(strObjectId) == kDeviceRootId)
        return _T("");

    // Walk upward collecting (objectId, name) pairs
    struct Crumb { CString id; CString name; };
    std::vector<Crumb> crumbs;

    CString current = strObjectId;
    while (!current.IsEmpty() && ToWide(current) != kDeviceRootId)
    {
        CString name = GetStringProperty(current, WPD_OBJECT_ORIGINAL_FILE_NAME);
        if (name.IsEmpty())
            name = GetStringProperty(current, WPD_OBJECT_NAME);
        if (name.IsEmpty())
            name = current;   // fallback to raw object-ID

        crumbs.push_back({ current, name });

        CString parent = GetStringProperty(current, WPD_OBJECT_PARENT_ID);

        // Guard against a broken device that reports itself as its own parent
        if (parent == current) break;
        current = parent;
    }

    // Reverse so the path reads root → leaf
    CString path;
    for (int i = (int)crumbs.size() - 1; i >= 0; --i)
    {
        if (!path.IsEmpty()) path += _T('/');
        path += crumbs[i].name;
    }
    return path;
}

// ── GetParentPath ─────────────────────────────────────────────────────────────
// Returns everything in the full path except the last component.
// e.g. for "Internal Storage/DCIM/Camera/photo.jpg"
//      returns "Internal Storage/DCIM/Camera"
//
// This is equivalent to std::filesystem::path(fullPath).parent_path().
// Implemented by getting the full path of the parent object, which is
// O(depth) WPD property reads — same cost as GetFullPath but one level less.

CString CWpdHelper::GetParentPath(const CString& strObjectId)
{
    CString parentId = GetParentObjectId(strObjectId);
    if (parentId.IsEmpty()) return _T("");   // already at root
    return GetFullPath(parentId);
}

// ── FindChild ─────────────────────────────────────────────────────────────────

CString CWpdHelper::FindChild(const CString& strParentObjectId, const CString& strName)
{
    // Local disk: delegate to std::filesystem, no cache needed
    if (m_bIsLocalDisk)
    {
        fs::path p = fs::path(strParentObjectId.GetString()) / strName.GetString();
        std::error_code ec;
        return fs::exists(p, ec) ? CString(p.c_str()) : CString(_T(""));
    }

    std::wstring parentKey = ToWide(strParentObjectId);

    // Populate cache entry on first access
    if (m_childCache.find(parentKey) == m_childCache.end())
    {
        ChildMap cm;
        auto children = GetChildren(strParentObjectId);
        for (auto& c : children)
        {
            std::wstring lowerName = ToWide(c.strName);
            for (auto& ch : lowerName) ch = towlower(ch);
            cm[lowerName] = c.strObjectId;
        }
        m_childCache[parentKey] = std::move(cm);
    }

    // Lookup
    std::wstring lowerName = ToWide(strName);
    for (auto& ch : lowerName) ch = towlower(ch);

    auto& cm = m_childCache[parentKey];
    auto  it = cm.find(lowerName);
    return it != cm.end() ? it->second : CString(_T(""));
}

void CWpdHelper::AddChild(const CString& strParentObjectId, const CString& strObjectName, const CString& strObjectId)
{
    if (m_bIsLocalDisk) return;

    std::wstring parentKey = ToWide(strParentObjectId);

    // Add a child map if the parent doesn't exist
    if (m_childCache.find(parentKey) == m_childCache.end())
    {
		ChildMap cm;
		std::wstring lowerName = ToWide(strObjectName);
		for (auto& ch : lowerName) ch = towlower(ch);
		cm[lowerName] = strObjectId;
		m_childCache[parentKey] = std::move(cm);
    }
    else
    {
		//If the parent already exists, add the child to the existing map
        auto& cm = m_childCache[parentKey];
        std::wstring lowerName = ToWide(strObjectName);
        for (auto& ch : lowerName) ch = towlower(ch);
		cm[lowerName] = strObjectId;
    }
}

// ── CreateFolder ──────────────────────────────────────────────────────────────

CString CWpdHelper::CreateFolder(const CString& strParentObjectId, const CString& strName)
{
    if (!m_pContent) return _T("");

    CComPtr<IPortableDeviceValues> pProps;
    if (FAILED(CoCreateInstance(CLSID_PortableDeviceValues, nullptr,
                                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pProps))))
        return _T("");

    std::wstring parentIdW = ToWide(strParentObjectId);
    std::wstring nameW     = ToWide(strName);

    pProps->SetStringValue(WPD_OBJECT_PARENT_ID, parentIdW.c_str());
    pProps->SetStringValue(WPD_OBJECT_NAME,       nameW.c_str());
    pProps->SetGuidValue  (WPD_OBJECT_CONTENT_TYPE, WPD_CONTENT_TYPE_FOLDER_GUID);
    pProps->SetGuidValue  (WPD_OBJECT_FORMAT,       WPD_OBJECT_FORMAT_PROPERTIES_ONLY_GUID);

    LPWSTR pwszNewId = nullptr;
    HRESULT hr = m_pContent->CreateObjectWithPropertiesOnly(pProps, &pwszNewId);

    CString newId;
    if (SUCCEEDED(hr) && pwszNewId)
    {
        newId = pwszNewId;
        CoTaskMemFree(pwszNewId);
    }
    return newId;
}

// ── EnumerateFiles ────────────────────────────────────────────────────────────

void CWpdHelper::EnumerateFiles(const CString& strRootObjectId,
                                 const CString& strRelPrefix,
                                 std::function<void(const MtpNode&, const CString&)> cb,
                                 std::function<bool(const MtpNode&)> skip)
{
    auto children = GetChildren(strRootObjectId);
    for (auto& node : children)
    {
        if (skip && skip(node)) continue;
        CString rel = strRelPrefix.IsEmpty()
                      ? node.strName
                      : strRelPrefix + _T("\\") + node.strName;
        if (node.bIsFolder)
            EnumerateFiles(node.strObjectId, rel, cb, skip);
        else
            cb(node, rel);
    }
}

// ── CopyFromDevice ────────────────────────────────────────────────────────────

bool CWpdHelper::CopyFromDevice(const CString& strObjectId,
                                 const CString& strLocalDestFile)
{
    if (!m_pContent) return false;

    CComPtr<IPortableDeviceResources> pResources;
    HRESULT hr = m_pContent->Transfer(&pResources);
    if (FAILED(hr)) return false;

    CComPtr<IStream> pStream;
    DWORD dwOptBuf = 0;
    hr = pResources->GetStream(ToWide(strObjectId).c_str(),
                               WPD_RESOURCE_DEFAULT,
                               STGM_READ, &dwOptBuf, &pStream);
    if (FAILED(hr)) return false;
    if (dwOptBuf == 0) dwOptBuf = 65536;

    // Ensure full destination path exists
    std::wstring destW = ToWide(strLocalDestFile);
    size_t sep = destW.rfind(L'\\');
    if (sep == std::wstring::npos) sep = destW.rfind(L'/');
    if (sep != std::wstring::npos)
        SHCreateDirectoryExW(nullptr, destW.substr(0, sep).c_str(), nullptr);

    HANDLE hFile = CreateFileW(destW.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    std::vector<BYTE> buf(dwOptBuf);
    bool ok = true;

    while (true)
    {
        ULONG cbRead = 0;
        hr = pStream->Read(buf.data(), dwOptBuf, &cbRead);
        if (cbRead > 0)
        {
            DWORD cbWritten = 0;
            if (!WriteFile(hFile, buf.data(), cbRead, &cbWritten, nullptr)
                || cbWritten != cbRead)
            { ok = false; break; }
        }
        if (hr == S_FALSE || cbRead == 0) break;
        if (FAILED(hr)) { ok = false; break; }
    }

    CloseHandle(hFile);
    if (!ok) DeleteFileW(destW.c_str());
    return ok;
}

// ── CopyToDevice ──────────────────────────────────────────────────────────────

CString CWpdHelper::CopyToDevice(const CString& strLocalSrcFile,
                                   const CString& strParentObjectId,
                                   const CString& strFileName)
{
    if (!m_pContent) return _T("");

    std::wstring srcW      = ToWide(strLocalSrcFile);
    std::wstring parentIdW = ToWide(strParentObjectId);
    std::wstring fileNameW = ToWide(strFileName);

    WIN32_FILE_ATTRIBUTE_DATA fad = {};
    if (!GetFileAttributesExW(srcW.c_str(), GetFileExInfoStandard, &fad))
        return _T("");
    ULONGLONG fileSize = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;

    size_t dot = fileNameW.rfind(L'.');
    std::wstring ext = (dot != std::wstring::npos) ? fileNameW.substr(dot + 1) : L"";
    for (auto& c : ext) c = (WCHAR)towlower(c);

    GUID fmtGuid = WPD_OBJECT_FORMAT_UNSPECIFIED_GUID;
    if      (ext == L"jpg" || ext == L"jpeg") fmtGuid = WPD_OBJECT_FORMAT_JFIF_GUID;
    else if (ext == L"png")                   fmtGuid = WPD_OBJECT_FORMAT_PNG_GUID;
    else if (ext == L"mp3")                   fmtGuid = WPD_OBJECT_FORMAT_MP3_GUID;
    else if (ext == L"mp4" || ext == L"mpeg" || ext == L"mpg")
                                              fmtGuid = WPD_OBJECT_FORMAT_MPEG_GUID;
    else if (ext == L"txt")                   fmtGuid = WPD_OBJECT_FORMAT_TEXT_GUID;

    CComPtr<IPortableDeviceValues> pObjProps;
    if (FAILED(CoCreateInstance(CLSID_PortableDeviceValues, nullptr,
                                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pObjProps))))
        return _T("");

    pObjProps->SetStringValue              (WPD_OBJECT_PARENT_ID,         parentIdW.c_str());
    pObjProps->SetStringValue              (WPD_OBJECT_ORIGINAL_FILE_NAME, fileNameW.c_str());
    pObjProps->SetStringValue              (WPD_OBJECT_NAME,               fileNameW.c_str());
    pObjProps->SetGuidValue                (WPD_OBJECT_CONTENT_TYPE,       WPD_CONTENT_TYPE_GENERIC_FILE_GUID);
    pObjProps->SetGuidValue                (WPD_OBJECT_FORMAT,             fmtGuid);
    pObjProps->SetUnsignedLargeIntegerValue(WPD_OBJECT_SIZE,               fileSize);

    CComPtr<IStream> pDevStream;
    DWORD dwOptBuf = 0;
    HRESULT hr = m_pContent->CreateObjectWithPropertiesAndData(
                     pObjProps, &pDevStream, &dwOptBuf, nullptr);
    if (FAILED(hr)) return _T("");
    if (dwOptBuf == 0) dwOptBuf = 65536;

    HANDLE hFile = CreateFileW(srcW.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        WCHAR msg[512];
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err, 0, msg, 512, nullptr);
        // Log or OutputDebugStringW to see the exact error
        OutputDebugStringW((std::wstring(L"CreateFileW failed: ") + msg).c_str());
        return _T("");
    }

    std::vector<BYTE> buf(dwOptBuf);
    DWORD cbRead = 0;
    bool  ok     = true;

    while (ReadFile(hFile, buf.data(), dwOptBuf, &cbRead, nullptr) && cbRead > 0)
    {
        ULONG cbWritten = 0;
        hr = pDevStream->Write(buf.data(), cbRead, &cbWritten);
        if (FAILED(hr) || cbWritten != cbRead) { ok = false; break; }
    }
    CloseHandle(hFile);

    CString newId;
    if (ok)
    {
        hr = pDevStream->Commit(STGC_DEFAULT);
        if (SUCCEEDED(hr))
        {
            CComQIPtr<IPortableDeviceDataStream> pDataStream(pDevStream);
            if (pDataStream)
            {
                LPWSTR pwszId = nullptr;
                if (SUCCEEDED(pDataStream->GetObjectID(&pwszId)) && pwszId)
                {
                    newId = pwszId;
                    CoTaskMemFree(pwszId);
                }
            }
        }
    }
    return newId;
}

// ── DeleteObject ──────────────────────────────────────────────────────────────

bool CWpdHelper::DeleteObject(const CString& strObjectId)
{
    if (!m_pContent) return false;

    CComPtr<IPortableDevicePropVariantCollection> pIds;
    if (FAILED(CoCreateInstance(CLSID_PortableDevicePropVariantCollection, nullptr,
                                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pIds))))
        return false;

    std::wstring idW = ToWide(strObjectId);
    PROPVARIANT  pv  = {};
    pv.vt            = VT_LPWSTR;
    pv.pwszVal       = const_cast<LPWSTR>(idW.c_str());
    pIds->Add(&pv);

    return SUCCEEDED(m_pContent->Delete(PORTABLE_DEVICE_DELETE_NO_RECURSION,
                                         pIds, nullptr));
}
