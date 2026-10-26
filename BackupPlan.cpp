#include "pch.h"
#include "BackupPlan.h"
#include "JsonLite.h"

using jsonlite::Value;

namespace PlanFile
{

const TCHAR* const kExtension     = _T(".lbp");
const TCHAR* const kDefaultPrefix = _T("LXBackupplan");

static const wchar_t* const kFormatName = L"LXFreeBack Backup Plan";
static const int            kVersion    = 1;

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::wstring W(const CString& s)  { return std::wstring(s.GetString()); }
static CString      C(const std::wstring& s) { return CString(s.c_str()); }

CString GetExeDir()
{
    std::vector<TCHAR> buf(MAX_PATH);
    for (;;)
    {
        DWORD n = ::GetModuleFileName(nullptr, buf.data(), (DWORD)buf.size());
        if (n == 0) return _T(".");
        if (n < buf.size() - 1) break;
        buf.resize(buf.size() * 2);
    }
    return DirOf(CString(buf.data()));
}

CString DirOf(const CString& path)
{
    int pos = path.ReverseFind(_T('\\'));
    int pos2 = path.ReverseFind(_T('/'));
    if (pos2 > pos) pos = pos2;
    if (pos < 0) return _T(".");
    CString dir = path.Left(pos);
    if (dir.GetLength() == 2 && dir[1] == _T(':')) dir += _T('\\');   // "C:" → "C:\"
    return dir;
}

CString NowIso8601()
{
    FILETIME ftUtc, ftLocal;
    ::GetSystemTimeAsFileTime(&ftUtc);
    ::FileTimeToLocalFileTime(&ftUtc, &ftLocal);

    ULARGE_INTEGER u, l;
    u.LowPart = ftUtc.dwLowDateTime;   u.HighPart = ftUtc.dwHighDateTime;
    l.LowPart = ftLocal.dwLowDateTime; l.HighPart = ftLocal.dwHighDateTime;
    long long diffMin = ((long long)l.QuadPart - (long long)u.QuadPart) / 600000000LL;

    SYSTEMTIME st;
    ::FileTimeToSystemTime(&ftLocal, &st);

    TCHAR sign = diffMin < 0 ? _T('-') : _T('+');
    long long a = diffMin < 0 ? -diffMin : diffMin;

    CString s;
    s.Format(_T("%04u-%02u-%02uT%02u:%02u:%02u%c%02lld:%02lld"),
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
             sign, a / 60, a % 60);
    return s;
}

CString FormatForDisplay(const CString& iso)
{
    if (iso.GetLength() < 16) return iso;
    CString s = iso.Left(16);         // "2026-10-26T17:05"
    s.SetAt(10, _T(' '));
    return s;
}

CString BuildPath(const CString& dir, const CString& title)
{
    CString d = dir;
    if (!d.IsEmpty() && d.Right(1) != _T("\\") && d.Right(1) != _T("/"))
        d += _T('\\');
    return d + title + kExtension;
}

CString MakeDefaultTitle(const std::vector<CString>& dirs)
{
    for (int n = 1; n < 100000; ++n)
    {
        CString title;
        title.Format(_T("%s%d"), kDefaultPrefix, n);
        bool taken = false;
        for (auto& d : dirs)
        {
            if (d.IsEmpty()) continue;
            if (::GetFileAttributes(BuildPath(d, title)) != INVALID_FILE_ATTRIBUTES)
            {
                taken = true;
                break;
            }
        }
        if (!taken) return title;
    }
    return kDefaultPrefix;
}

bool ValidateTitle(const CString& title, CString& err)
{
    if (title.IsEmpty())
    {
        err = _T("The plan title is empty.");
        return false;
    }
    if (title.FindOneOf(_T("\\/:*?\"<>|")) >= 0)
    {
        err = _T("The plan title is used as the file name, so it cannot contain any of these characters:\n\\ / : * ? \" < > |");
        return false;
    }
    for (int i = 0; i < title.GetLength(); ++i)
    {
        if (title[i] < 32)
        {
            err = _T("The plan title contains control characters.");
            return false;
        }
    }
    TCHAR last = title[title.GetLength() - 1];
    if (last == _T('.') || last == _T(' '))
    {
        err = _T("The plan title cannot end with a dot or a space.");
        return false;
    }
    static const TCHAR* reserved[] = {
        _T("CON"), _T("PRN"), _T("AUX"), _T("NUL"),
        _T("COM1"), _T("COM2"), _T("COM3"), _T("COM4"), _T("COM5"),
        _T("COM6"), _T("COM7"), _T("COM8"), _T("COM9"),
        _T("LPT1"), _T("LPT2"), _T("LPT3"), _T("LPT4"), _T("LPT5"),
        _T("LPT6"), _T("LPT7"), _T("LPT8"), _T("LPT9") };
    for (auto r : reserved)
    {
        if (title.CompareNoCase(r) == 0)
        {
            err.Format(_T("\"%s\" is a reserved Windows device name and cannot be used as a plan title."),
                       (LPCTSTR)title);
            return false;
        }
    }
    return true;
}

CString DescribeEndpoint(const CEndpoint& ep)
{
    CString tag = ep.bIsLocal ? _T("[Local] ") : _T("[MTP] ");
    return tag + ep.strDisplayPath;
}

CString ListDisplay(const CEndpoint& ep, const std::vector<CSourceItem>& list)
{
    if (list.empty()) return CString();
    const CSourceItem& first = list.front();
    CString where = !first.strSubPath.IsEmpty() ? first.strSubPath
                  : !first.strLocalPath.IsEmpty() ? first.strLocalPath : first.strName;
    CString s = ep.strDeviceName.IsEmpty() ? where : ep.strDeviceName + _T(" / ") + where;
    if (list.size() > 1)
    {
        CString more;
        more.Format(_T("  (+%d more)"), (int)list.size() - 1);
        s += more;
    }
    return s;
}

CString DestDisplay(const CEndpoint& ep)
{
    CString where = !ep.strSubPath.IsEmpty() ? ep.strSubPath : ep.strLocalPath;
    return ep.strDeviceName.IsEmpty() ? where : ep.strDeviceName + _T(" / ") + where;
}

std::vector<CCopyTask> ToTasks(const CBackupPlan& plan)
{
    std::vector<CCopyTask> tasks;
    for (auto& it : plan.items)
        tasks.push_back(CCopyTask{ it.src, it.dst, it.nOnConflict });
    return tasks;
}

// ── JSON mapping ──────────────────────────────────────────────────────────────

// Only the device NAME and the path below the device root are stored.
// Device IDs, object IDs and absolute paths are looked up again at run time.

static Value EntriesToJson(const std::vector<CSourceItem>& list)
{
    Value entries = Value::MakeArray();
    for (auto& it : list)
    {
        Value e = Value::MakeObject();
        e.Set(L"name",     Value(W(it.strName)));
        e.Set(L"isFolder", Value(it.bIsFolder));
        e.Set(L"subPath",  Value(W(it.strSubPath)));
        entries.Append(e);
    }
    return entries;
}

static void EntriesFromJson(const Value* entries, const CEndpoint& ep,
                            std::vector<CSourceItem>& out)
{
    if (!entries || !entries->IsArray()) return;
    for (auto& e : entries->Items())
    {
        if (!e.IsObject()) continue;
        CSourceItem si;
        si.strName    = C(e.GetString(L"name"));
        si.bIsFolder  = e.GetBool(L"isFolder");
        si.strSubPath = C(e.GetString(L"subPath"));

        // Older plan files: "mtpPath"/"objectId" (MTP) or absolute "path" (local)
        if (si.strSubPath.IsEmpty())
        {
            if (ep.bIsLocal)
            {
                CString path = C(e.GetString(L"path"));
                si.strSubPath = SubPathBelow(ep.strDeviceId, path);
                if (si.strSubPath.IsEmpty()) { si.strLocalPath = path; si.strObjectId = path; }
            }
            else
            {
                si.strSubPath  = C(e.GetString(L"mtpPath"));
                if (si.strSubPath.IsEmpty()) si.strObjectId = C(e.GetString(L"objectId"));
            }
        }
        if (si.strName.IsEmpty())
        {
            CString p = si.strSubPath.IsEmpty() ? si.strLocalPath : si.strSubPath;
            p.Replace(_T('/'), _T('\\'));
            p.TrimRight(_T('\\'));
            int pos = p.ReverseFind(_T('\\'));
            si.strName = pos >= 0 ? p.Mid(pos + 1) : p;
        }
        out.push_back(si);
    }
}

static Value EndpointToJson(const CEndpoint& ep, bool bSource)
{
    Value o = Value::MakeObject();
    o.Set(L"type",       Value(ep.bIsLocal ? L"local" : L"mtp"));
    o.Set(L"deviceName", Value(W(ep.strDeviceName)));
    if (bSource)
    {
        o.Set(L"entries",    EntriesToJson(ep.items));
        o.Set(L"exclusions", EntriesToJson(ep.excludes));
    }
    else
    {
        o.Set(L"subPath", Value(W(ep.strSubPath)));
    }
    return o;
}

static bool EndpointFromJson(const Value* v, bool bSource, CEndpoint& ep, CString& err)
{
    if (!v || !v->IsObject())
    {
        err = bSource ? _T("an item has no \"source\" object")
                      : _T("an item has no \"destination\" object");
        return false;
    }
    std::wstring type = v->GetString(L"type", L"local");
    ep = CEndpoint{};
    ep.bIsLocal      = (type != L"mtp");
    ep.strDeviceName = C(v->GetString(L"deviceName"));
    ep.strDeviceId   = C(v->GetString(L"deviceId"));   // older files only; never written

    if (bSource)
    {
        EntriesFromJson(v->Find(L"entries"),    ep, ep.items);
        EntriesFromJson(v->Find(L"exclusions"), ep, ep.excludes);
        ep.strDisplayPath = ListDisplay(ep, ep.items);
    }
    else
    {
        ep.strSubPath = C(v->GetString(L"subPath"));
        if (ep.strSubPath.IsEmpty())
        {
            if (ep.bIsLocal)
            {
                CString path = C(v->GetString(L"path"));
                ep.strSubPath = SubPathBelow(ep.strDeviceId, path);
                if (ep.strSubPath.IsEmpty()) ep.strLocalPath = path;
            }
            else
            {
                ep.strSubPath = C(v->GetString(L"mtpPath"));
                if (ep.strSubPath.IsEmpty()) ep.strObjectId = C(v->GetString(L"objectId"));
            }
        }
        ep.strDisplayPath = DestDisplay(ep);
    }
    return true;
}

// ── File I/O ──────────────────────────────────────────────────────────────────

CString ErrorText(DWORD code)
{
    LPTSTR buf = nullptr;
    ::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS,
                    nullptr, code, 0, (LPTSTR)&buf, 0, nullptr);
    CString s = buf ? CString(buf) : CString();
    if (buf) ::LocalFree(buf);
    s.TrimRight();
    if (s.IsEmpty()) s.Format(_T("Error %lu"), code);
    return s;
}

bool Save(const CBackupPlan& plan, const CString& path, CString& err)
{
    Value root = Value::MakeObject();
    root.Set(L"format",   Value(kFormatName));
    root.Set(L"version",  Value(kVersion));
    root.Set(L"name",     Value(W(plan.strTitle)));
    root.Set(L"created",  Value(W(plan.strCreated)));
    root.Set(L"modified", Value(W(plan.strModified)));

    Value items = Value::MakeArray();
    for (auto& it : plan.items)
    {
        Value o = Value::MakeObject();
        o.Set(L"source",      EndpointToJson(it.src, true));
        o.Set(L"onConflict",  Value(it.nOnConflict == CONFLICT_OVERWRITE ? L"overwrite"
                                  : it.nOnConflict == CONFLICT_SKIP      ? L"skip"
                                                                         : L"overwriteIfNewer"));
        o.Set(L"destination", EndpointToJson(it.dst, false));
        items.Append(o);
    }
    root.Set(L"items", items);

    std::string text = jsonlite::Write(root, 2);

    // Write to a temporary file first, then replace — a crash never leaves a
    // half-written plan behind.
    CString tmp = path + _T(".tmp");
    HANDLE h = ::CreateFile(tmp, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
    {
        err = _T("Cannot create ") + path + _T(":\n") + ErrorText(::GetLastError());
        return false;
    }
    DWORD written = 0;
    BOOL ok = ::WriteFile(h, text.data(), (DWORD)text.size(), &written, nullptr)
              && written == (DWORD)text.size();
    DWORD werr = ::GetLastError();
    ::CloseHandle(h);
    if (!ok)
    {
        ::DeleteFile(tmp);
        err = _T("Cannot write ") + path + _T(":\n") + ErrorText(werr);
        return false;
    }
    if (!::MoveFileEx(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DWORD merr = ::GetLastError();
        ::DeleteFile(tmp);
        err = _T("Cannot save ") + path + _T(":\n") + ErrorText(merr);
        return false;
    }
    return true;
}

bool Load(const CString& path, CBackupPlan& plan, CString& err)
{
    HANDLE h = ::CreateFile(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
    {
        err = _T("Cannot open ") + path + _T(":\n") + ErrorText(::GetLastError());
        return false;
    }
    LARGE_INTEGER size = {};
    ::GetFileSizeEx(h, &size);
    if (size.QuadPart > 32LL * 1024 * 1024)
    {
        ::CloseHandle(h);
        err = path + _T(" is too large to be a backup plan.");
        return false;
    }
    std::string text((size_t)size.QuadPart, '\0');
    DWORD read = 0;
    BOOL ok = text.empty() ||
              ::ReadFile(h, &text[0], (DWORD)text.size(), &read, nullptr);
    ::CloseHandle(h);
    if (!ok)
    {
        err = _T("Cannot read ") + path;
        return false;
    }
    text.resize(read);

    Value root;
    std::wstring jerr;
    if (!jsonlite::Parse(text, root, &jerr))
    {
        err = path + _T(" is not a valid backup plan.\n") + C(jerr);
        return false;
    }
    if (!root.IsObject())
    {
        err = path + _T(" is not a valid backup plan (the top level is not a JSON object).");
        return false;
    }
    const Value* items = root.Find(L"items");
    if (!items || !items->IsArray())
    {
        err = path + _T(" is not a valid backup plan (no \"items\" list).");
        return false;
    }
    double version = root.Find(L"version") ? root.Find(L"version")->AsNumber(kVersion) : kVersion;
    if (version > kVersion)
    {
        err.Format(_T("%s was written by a newer version of LXFreeBack (format version %d)."),
                   (LPCTSTR)path, (int)version);
        return false;
    }

    CBackupPlan p;
    p.strTitle    = C(root.GetString(L"name"));
    p.strCreated  = C(root.GetString(L"created"));
    p.strModified = C(root.GetString(L"modified"));
    p.strFilePath = path;

    if (p.strTitle.IsEmpty())
    {
        // Fall back to the file name without extension
        CString name = path.Mid(path.ReverseFind(_T('\\')) + 1);
        int dot = name.ReverseFind(_T('.'));
        p.strTitle = dot > 0 ? name.Left(dot) : name;
    }

    int idx = 0;
    for (auto& v : items->Items())
    {
        ++idx;
        if (!v.IsObject()) continue;
        CPlanItem item;
        CString e;
        if (!EndpointFromJson(v.Find(L"source"), true, item.src, e) ||
            !EndpointFromJson(v.Find(L"destination"), false, item.dst, e))
        {
            err.Format(_T("%s is not a valid backup plan (item %d: %s)."),
                       (LPCTSTR)path, idx, (LPCTSTR)e);
            return false;
        }
        std::wstring oc = v.GetString(L"onConflict", L"overwriteIfNewer");
        item.nOnConflict = oc == L"overwrite" ? CONFLICT_OVERWRITE
                         : oc == L"skip"      ? CONFLICT_SKIP
                                              : CONFLICT_NEWER;   // also for older files
        p.items.push_back(std::move(item));
    }

    plan = std::move(p);
    return true;
}

} // namespace PlanFile
