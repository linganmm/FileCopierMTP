#include "pch.h"
#include "ItemDlg.h"

BEGIN_MESSAGE_MAP(CItemDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_BROWSE_SRC,  &CItemDlg::OnBtnBrowseSrc)
    ON_BN_CLICKED(IDC_BTN_BROWSE_DST,  &CItemDlg::OnBtnBrowseDst)
    ON_BN_CLICKED(IDC_BTN_BROWSE_EXCL, &CItemDlg::OnBtnBrowseExcl)
    ON_BN_CLICKED(IDC_BTN_CLEAR_EXCL,  &CItemDlg::OnBtnClearExcl)
END_MESSAGE_MAP()

CItemDlg::CItemDlg(CBrowserState* pSrcState, CBrowserState* pExclState,
                   CBrowserState* pDstState, bool bEditMode, CWnd* pParent)
    : CDialogEx(IDD_ITEM_DIALOG, pParent)
    , m_pSrcState(pSrcState)
    , m_pExclState(pExclState)
    , m_pDstState(pDstState)
    , m_bEditMode(bEditMode)
{}

void CItemDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_SRC_PATH,    m_editSrc);
    DDX_Control(pDX, IDC_EDIT_DST_PATH,    m_editDst);
    DDX_Control(pDX, IDC_STATIC_SRC_TYPE,  m_staticSrcType);
    DDX_Control(pDX, IDC_STATIC_DST_TYPE,  m_staticDstType);
    DDX_Control(pDX, IDC_EDIT_EXCL_PATH,   m_editExcl);
    DDX_Control(pDX, IDC_STATIC_EXCL_TYPE, m_staticExclType);
    DDX_Control(pDX, IDC_COMBO_CONFLICT,   m_comboConflict);
    DDX_Control(pDX, IDC_COMBO_MODIFIED,   m_comboModified);
}

BOOL CItemDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    SetWindowText(m_bEditMode ? _T("Edit Item") : _T("Add Item"));

    // Same choices as the "File Conflict" dialog; item data = CONFLICT_* value
    const int policies[] = { CONFLICT_OVERWRITE, CONFLICT_SKIP, CONFLICT_NEWER };
    for (int p : policies)
    {
        int i = m_comboConflict.AddString(ConflictPolicyName(p));
        m_comboConflict.SetItemData(i, (DWORD_PTR)p);
        if (p == m_item.nOnConflict) m_comboConflict.SetCurSel(i);
    }
    if (m_comboConflict.GetCurSel() < 0) m_comboConflict.SetCurSel(2);   // Overwrite if newer

    // "Modified within": blank (= any time, the default), 1 day, 1 week, 1 month
    const int periods[] = { MODIFIED_ANY, MODIFIED_DAY, MODIFIED_WEEK, MODIFIED_MONTH };
    for (int p : periods)
    {
        int i = m_comboModified.AddString(ModifiedWithinName(p));
        m_comboModified.SetItemData(i, (DWORD_PTR)p);
        if (p == m_item.nModifiedWithin) m_comboModified.SetCurSel(i);
    }
    if (m_comboModified.GetCurSel() < 0) m_comboModified.SetCurSel(0);

    m_editExcl.SetLimitText(4096);
    WriteExclusionBox();
    UpdateUi();
    return TRUE;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

CString CItemDlg::DeviceName(const CBrowserState* pState, const CString& deviceId)
{
    if (pState)
        for (auto& d : pState->devices)
            if (d.strDeviceId == deviceId) return d.strFriendlyName;
    return CString();
}

// Device the browser state was last showing ("" if unknown)
CString CItemDlg::StateDeviceId(const CBrowserState* pState)
{
    if (!pState || !pState->bInitialized) return CString();
    int i = pState->nLastDeviceIdx;
    if (i < 0 || i >= (int)pState->devices.size()) return CString();
    return pState->devices[i].strDeviceId;
}

// Same device?  Loaded plans carry only the device name, so compare IDs
// when both are known and names otherwise.
static bool SameDevice(bool localA, const CString& idA, const CString& nameA,
                       bool localB, const CString& idB, const CString& nameB)
{
    if (localA != localB) return false;
    if (!idA.IsEmpty() && !idB.IsEmpty()) return idA.CompareNoCase(idB) == 0;
    return !nameA.IsEmpty() && nameA == nameB;
}

// Fill the device-relative sub path of freshly browsed items.
static void FillSubPaths(std::vector<CSourceItem>& list, bool bLocal, const CString& deviceId)
{
    if (bLocal)
    {
        for (auto& it : list)
        {
            if (it.strLocalPath.IsEmpty()) it.strLocalPath = it.strObjectId;
            it.strSubPath = SubPathBelow(deviceId, it.strLocalPath);
        }
    }
    else
    {
        // Object IDs change between connections — record the name path.
        CWaitCursor wait;
        CWpdHelper wpd;
        if (wpd.OpenDevice(deviceId))
            for (auto& it : list)
                it.strSubPath = wpd.GetFullPath(it.strObjectId);
    }
}

// Normalised path for prefix tests: '\' separators (local) or '/' (MTP),
// no trailing separator except on a drive root.
static CString NormPath(const CString& path, TCHAR sep)
{
    CString p = path;
    p.Replace(sep == _T('\\') ? _T('/') : _T('\\'), sep);
    while (p.GetLength() > 3 && p[p.GetLength() - 1] == sep)
        p = p.Left(p.GetLength() - 1);
    return p;
}

// True if x is one of the source items or lies inside a source folder.
bool CItemDlg::IsInsideSource(const CEndpoint& src, const CSourceItem& x)
{
    const TCHAR sep = src.bIsLocal ? _T('\\') : _T('/');
    for (auto& it : src.items)
    {
        CString a = x.strSubPath.IsEmpty()  ? x.strLocalPath  : x.strSubPath;
        CString b = it.strSubPath.IsEmpty() ? it.strLocalPath : it.strSubPath;
        if (a.IsEmpty() || b.IsEmpty())
        {
            if (!x.strObjectId.IsEmpty() && x.strObjectId == it.strObjectId) return true;
            continue;
        }
        a = NormPath(a, sep);
        b = NormPath(b, sep);
        if (a.CompareNoCase(b) == 0) return true;
        if (!it.bIsFolder) continue;
        if (a.GetLength() > b.GetLength() &&
            a.Left(b.GetLength()).CompareNoCase(b) == 0 &&
            (b[b.GetLength() - 1] == sep || a[b.GetLength()] == sep))
            return true;
    }
    return false;
}

// Exclusions only make sense inside the source folders; drop the rest.
void CItemDlg::DropOutsideExclusions(bool bTellUser)
{
    auto& ex = m_item.src.excludes;
    std::vector<CSourceItem> keep;
    CString dropped;
    int nDropped = 0;
    for (auto& x : ex)
    {
        if (IsInsideSource(m_item.src, x)) { keep.push_back(x); continue; }
        if (++nDropped <= 8)
        {
            CString p = m_item.src.bIsLocal ? x.strLocalPath : x.strSubPath;
            dropped += _T("\n  ") + (p.IsEmpty() ? x.strName : p);
        }
    }
    if (nDropped == 0) return;
    ex.swap(keep);
    if (nDropped > 8)
    {
        CString more;
        more.Format(_T("\n  \u2026 and %d more"), nDropped - 8);
        dropped += more;
    }
    if (bTellUser)
        MessageBox(_T("These exclusions are not inside the selected source and were removed:") + dropped,
                   _T("Exclusion"), MB_ICONINFORMATION);
}

// Split on commas that are outside "quotes" and outside (), [] and {} — so a
// regex like a{1,3} stays whole.  Outside quotes a backslash protects the next
// character, so "\," is a literal comma inside a pattern.
std::vector<CString> CItemDlg::SplitConditions(const CString& text)
{
    std::vector<CString> out;
    CString cur;
    bool inQuote = false;
    int  depth   = 0;
    for (int i = 0; i < text.GetLength(); ++i)
    {
        TCHAR c = text[i];
        if (inQuote)
        {
            cur += c;
            if (c == _T('"')) inQuote = false;
            continue;
        }
        if (c == _T('\\') && i + 1 < text.GetLength())
        {
            cur += c;
            cur += text[++i];
            continue;
        }
        if (c == _T('"')) inQuote = true;
        else if (c == _T('(') || c == _T('[') || c == _T('{')) ++depth;
        else if ((c == _T(')') || c == _T(']') || c == _T('}')) && depth > 0) --depth;
        else if (c == _T(',') && depth == 0)
        {
            cur.Trim();
            if (!cur.IsEmpty()) out.push_back(cur);
            cur.Empty();
            continue;
        }
        cur += c;
    }
    cur.Trim();
    if (!cur.IsEmpty()) out.push_back(cur);
    return out;
}

bool CItemDlg::IsAbsoluteLocal(const CString& path)
{
    return path.GetLength() >= 2 && path[1] == _T(':');
}

CString CItemDlg::ExclusionPathText(const CSourceItem& x)
{
    return x.strSubPath.IsEmpty() ? x.strLocalPath : x.strSubPath;
}

void CItemDlg::WriteExclusionBox()
{
    CString text;
    for (auto& x : m_item.src.excludes)
    {
        if (!text.IsEmpty()) text += _T(", ");
        text += _T("\"") + ExclusionPathText(x) + _T("\"");
    }
    for (auto& p : m_item.src.excludePatterns)
    {
        if (!text.IsEmpty()) text += _T(", ");
        text += p;
    }
    m_editExcl.SetWindowText(text);
    m_staticExclType.SetWindowText(m_item.src.excludes.empty() ? _T("")
                                   : (m_item.src.bIsLocal ? _T("[Local]") : _T("[MTP]")));
}

void CItemDlg::ReadExclusionBox()
{
    CString text;
    m_editExcl.GetWindowText(text);

    std::vector<CSourceItem> paths;
    std::vector<CString>     patterns;
    for (auto& tok : SplitConditions(text))
    {
        if (tok.GetLength() >= 2 && tok[0] == _T('"') && tok[tok.GetLength() - 1] == _T('"'))
        {
            CString path = tok.Mid(1, tok.GetLength() - 2);
            path.Trim();
            if (path.IsEmpty()) continue;

            // Keep the full record of an exclusion picked with Browse...
            bool found = false;
            for (auto& x : m_item.src.excludes)
                if (ExclusionPathText(x).CompareNoCase(path) == 0) { paths.push_back(x); found = true; break; }
            if (found) continue;

            // ...or build one from a typed path
            CSourceItem x;
            if (m_item.src.bIsLocal)
            {
                path.Replace(_T('/'), _T('\\'));
                if (IsAbsoluteLocal(path))
                {
                    x.strLocalPath = path;
                    x.strObjectId  = path;
                    x.strSubPath   = SubPathBelow(m_item.src.strDeviceId, path);
                }
                else
                    x.strSubPath = path;
            }
            else
            {
                path.Replace(_T('\\'), _T('/'));
                x.strSubPath = path;
            }
            CString n = path;
            n.Replace(_T('/'), _T('\\'));
            n.TrimRight(_T('\\'));
            int pos = n.ReverseFind(_T('\\'));
            x.strName = pos >= 0 ? n.Mid(pos + 1) : n;
            paths.push_back(x);
        }
        else
            patterns.push_back(tok);
    }
    m_item.src.excludes        = paths;
    m_item.src.excludePatterns = patterns;
}

bool CItemDlg::SourceOk(const CEndpoint& ep)
{
    return !ep.items.empty();
}

bool CItemDlg::DestOk(const CEndpoint& ep)
{
    if (ep.bIsLocal) return !ep.strLocalPath.IsEmpty() || !ep.strSubPath.IsEmpty();
    return (!ep.strDeviceId.IsEmpty() || !ep.strDeviceName.IsEmpty()) &&
           (!ep.strObjectId.IsEmpty() || !ep.strSubPath.IsEmpty());
}

void CItemDlg::UpdateUi()
{
    bool src  = SourceOk(m_item.src);
    bool dst  = DestOk(m_item.dst);
    m_editSrc.SetWindowText(src ? m_item.src.strDisplayPath : CString());
    m_editDst.SetWindowText(dst ? m_item.dst.strDisplayPath : CString());
    m_staticSrcType.SetWindowText(!src ? _T("") : (m_item.src.bIsLocal ? _T("[Local]") : _T("[MTP]")));
    m_staticDstType.SetWindowText(!dst ? _T("") : (m_item.dst.bIsLocal ? _T("[Local]") : _T("[MTP]")));
    GetDlgItem(IDC_BTN_BROWSE_EXCL)->EnableWindow(src);
    GetDlgItem(IDC_BTN_CLEAR_EXCL)->EnableWindow(src);
    m_editExcl.EnableWindow(src);
    GetDlgItem(IDOK)->EnableWindow(src && dst);
}

// ── Browse ────────────────────────────────────────────────────────────────────

void CItemDlg::OnBtnBrowseSrc()
{
    // A new item starts with nothing highlighted, even though the tree keeps
    // the folders expanded from the previous item.
    if (!m_bSrcBrowsed && m_pSrcState)
        for (auto& snap : m_pSrcState->treeSnapshot) snap.bSelected = false;
    m_bSrcBrowsed = true;

    ReadExclusionBox();                 // keep what was typed

    CMtpBrowserDlg dlg(this);
    dlg.m_bSourceMode = true;
    dlg.m_pState      = m_pSrcState;
    if (dlg.DoModal() != IDOK) return;

    CEndpoint ep;
    ep.bIsLocal      = dlg.m_bIsLocalDisk;
    ep.strDeviceId   = dlg.m_strSelectedDeviceId;
    ep.strDeviceName = DeviceName(m_pSrcState, ep.strDeviceId);
    ep.items         = dlg.m_selectedItems;

    FillSubPaths(ep.items, ep.bIsLocal, ep.strDeviceId);
    ep.strDisplayPath = PlanFile::ListDisplay(ep, ep.items);

    // Keep the exclusions that still fall inside the new source
    bool sameDevice = SameDevice(ep.bIsLocal, ep.strDeviceId, ep.strDeviceName,
                                 m_item.src.bIsLocal, m_item.src.strDeviceId,
                                 m_item.src.strDeviceName);
    if (sameDevice) ep.excludes = m_item.src.excludes;
    ep.excludePatterns = m_item.src.excludePatterns;   // name patterns apply anywhere

    m_item.src = ep;
    DropOutsideExclusions(true);
    WriteExclusionBox();
    UpdateUi();
}

void CItemDlg::OnBtnBrowseExcl()
{
    if (!SourceOk(m_item.src))
    {
        MessageBox(_T("Select the source first. Exclusions are picked from inside the source folders."),
                   _T("Exclusion"), MB_ICONINFORMATION);
        return;
    }
    ReadExclusionBox();                 // keep typed patterns and paths

    // Open the browser where the source is: start from the source tree
    // (already expanded down to the source folders) unless the exclusion
    // browser is already showing that device.
    if (m_pExclState)
    {
        CString exclDev = StateDeviceId(m_pExclState);
        CString srcDev  = StateDeviceId(m_pSrcState);
        bool exclOnSource = SameDevice(m_item.src.bIsLocal, exclDev, DeviceName(m_pExclState, exclDev),
                                       m_item.src.bIsLocal, m_item.src.strDeviceId, m_item.src.strDeviceName);
        bool srcOnSource  = SameDevice(m_item.src.bIsLocal, srcDev, DeviceName(m_pSrcState, srcDev),
                                       m_item.src.bIsLocal, m_item.src.strDeviceId, m_item.src.strDeviceName);
        if (!exclOnSource && srcOnSource && m_pSrcState)
        {
            *m_pExclState = *m_pSrcState;
            m_pExclState->strLastSelDisplayPath.Empty();
        }
        // Highlight the current exclusions (and nothing else)
        for (auto& snap : m_pExclState->treeSnapshot)
        {
            snap.bSelected = false;
            for (auto& x : m_item.src.excludes)
            {
                if (!x.strObjectId.IsEmpty() && x.strObjectId == snap.item.strObjectId)
                    snap.bSelected = true;
                // Plans loaded from disk only know the sub path (local drives)
                else if (m_item.src.bIsLocal && !x.strSubPath.IsEmpty() &&
                         x.strSubPath.CompareNoCase(
                             SubPathBelow(StateDeviceId(m_pExclState), snap.item.strLocalPath)) == 0)
                    snap.bSelected = true;
            }
        }
    }

    CMtpBrowserDlg dlg(this);
    dlg.m_bSourceMode = true;
    dlg.m_pState      = m_pExclState;
    dlg.m_strCaption  = _T("Select Files / Folders to Exclude");
    if (dlg.DoModal() != IDOK) return;

    CString pickedId = dlg.m_strSelectedDeviceId;
    if (!SameDevice(dlg.m_bIsLocalDisk, pickedId, DeviceName(m_pExclState, pickedId),
                    m_item.src.bIsLocal, m_item.src.strDeviceId, m_item.src.strDeviceName))
    {
        CString name = m_item.src.strDeviceName.IsEmpty() ? m_item.src.strDeviceId
                                                          : m_item.src.strDeviceName;
        MessageBox(_T("Exclusions must be on the same device / drive as the source (") + name + _T(")."),
                   _T("Exclusion"), MB_ICONWARNING);
        return;
    }

    std::vector<CSourceItem> list = dlg.m_selectedItems;
    FillSubPaths(list, m_item.src.bIsLocal, pickedId);
    if (m_item.src.strDeviceId.IsEmpty()) m_item.src.strDeviceId = pickedId;   // loaded plan

    m_item.src.excludes = list;            // the selection replaces the old paths;
    DropOutsideExclusions(true);           // typed patterns are kept
    WriteExclusionBox();
    UpdateUi();
}

void CItemDlg::OnBtnClearExcl()
{
    m_item.src.excludes.clear();
    m_item.src.excludePatterns.clear();
    WriteExclusionBox();
    UpdateUi();
}

void CItemDlg::OnBtnBrowseDst()
{
    // Don't offer a stale "selected path" that has no folder behind it
    if (m_pDstState) m_pDstState->strLastSelDisplayPath.Empty();

    CMtpBrowserDlg dlg(this);
    dlg.m_bSourceMode = false;
    dlg.m_pState      = m_pDstState;
    if (dlg.DoModal() != IDOK) return;

    CEndpoint ep;
    ep.bIsLocal      = dlg.m_bIsLocalDisk;
    ep.strDeviceId   = dlg.m_strSelectedDeviceId;
    ep.strDeviceName = DeviceName(m_pDstState, ep.strDeviceId);

    if (ep.bIsLocal)
    {
        ep.strLocalPath = dlg.m_strSelectedLocalPath;
        ep.strSubPath   = SubPathBelow(ep.strDeviceId, ep.strLocalPath);
    }
    else
    {
        ep.strObjectId = dlg.m_strSelectedObjectId;
        CWaitCursor wait;
        CWpdHelper wpd;
        if (wpd.OpenDevice(ep.strDeviceId))
            ep.strSubPath = wpd.GetFullPath(ep.strObjectId);
    }
    ep.strDisplayPath = PlanFile::DestDisplay(ep);
    if (ep.strDisplayPath.IsEmpty()) ep.strDisplayPath = dlg.m_strSelectedDisplayPath;

    m_item.dst = ep;
    UpdateUi();
}

void CItemDlg::OnOK()
{
    if (!SourceOk(m_item.src))
    {
        MessageBox(_T("Please select source files/folders."), _T("Error"), MB_ICONWARNING);
        return;
    }
    if (!DestOk(m_item.dst))
    {
        MessageBox(_T("Please select a destination folder."), _T("Error"), MB_ICONWARNING);
        return;
    }

    // Exclusions: valid regular expressions, paths inside the source
    ReadExclusionBox();
    for (auto& p : m_item.src.excludePatterns)
    {
        try
        {
            std::wregex re(std::wstring(p.GetString()),
                           std::regex_constants::ECMAScript | std::regex_constants::icase);
        }
        catch (const std::regex_error&)
        {
            MessageBox(_T("This exclusion is not a valid regular expression:\n\n  ") + p +
                       _T("\n\nPut exact paths in double quotes, e.g. \"DCIM\\a3\"."),
                       _T("Exclusion"), MB_ICONWARNING);
            m_editExcl.SetFocus();
            return;
        }
    }
    size_t before = m_item.src.excludes.size();
    DropOutsideExclusions(true);
    if (m_item.src.excludes.size() != before)
    {
        WriteExclusionBox();               // show what is left, let the user confirm
        return;
    }

    int sel = m_comboConflict.GetCurSel();
    m_item.nOnConflict = sel >= 0 ? (int)m_comboConflict.GetItemData(sel) : CONFLICT_NEWER;
    sel = m_comboModified.GetCurSel();
    m_item.nModifiedWithin = sel >= 0 ? (int)m_comboModified.GetItemData(sel) : MODIFIED_ANY;
    CDialogEx::OnOK();
}
