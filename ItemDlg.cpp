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

CString CItemDlg::ListDisplay(const CEndpoint& ep, const std::vector<CSourceItem>& list)
{
    if (list.empty()) return CString();
    const CSourceItem& first = list.front();
    CString s;
    if (ep.bIsLocal)
        s = first.strLocalPath.IsEmpty() ? first.strName : first.strLocalPath;
    else
        s = ep.strDeviceName + _T(" / ") +
            (first.strMtpPath.IsEmpty() ? first.strName : first.strMtpPath);
    int n = (int)list.size();
    if (n > 1)
    {
        CString more;
        more.Format(_T("  (+%d more)"), n - 1);
        s += more;
    }
    return s;
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
    for (auto& it : src.items)
    {
        CString a, b;
        TCHAR sep;
        if (src.bIsLocal)
        {
            sep = _T('\\');
            a = NormPath(x.strLocalPath.IsEmpty() ? x.strObjectId : x.strLocalPath, sep);
            b = NormPath(it.strLocalPath.IsEmpty() ? it.strObjectId : it.strLocalPath, sep);
        }
        else
        {
            if (x.strMtpPath.IsEmpty() || it.strMtpPath.IsEmpty())
            {
                if (x.strObjectId == it.strObjectId) return true;
                continue;
            }
            sep = _T('/');
            a = NormPath(x.strMtpPath, sep);
            b = NormPath(it.strMtpPath, sep);
        }
        if (b.IsEmpty()) continue;
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
            CString p = m_item.src.bIsLocal ? x.strLocalPath : x.strMtpPath;
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

bool CItemDlg::SourceOk(const CEndpoint& ep)
{
    return !ep.items.empty();
}

bool CItemDlg::DestOk(const CEndpoint& ep)
{
    if (ep.bIsLocal) return !ep.strLocalPath.IsEmpty();
    return !ep.strDeviceId.IsEmpty() &&
           (!ep.strObjectId.IsEmpty() || !ep.strMtpPath.IsEmpty());
}

void CItemDlg::UpdateUi()
{
    bool src  = SourceOk(m_item.src);
    bool dst  = DestOk(m_item.dst);
    bool excl = src && !m_item.src.excludes.empty();
    m_editSrc.SetWindowText(src ? m_item.src.strDisplayPath : CString());
    m_editDst.SetWindowText(dst ? m_item.dst.strDisplayPath : CString());
    m_editExcl.SetWindowText(excl ? ListDisplay(m_item.src, m_item.src.excludes) : CString());
    m_staticSrcType.SetWindowText(!src ? _T("") : (m_item.src.bIsLocal ? _T("[Local]") : _T("[MTP]")));
    m_staticDstType.SetWindowText(!dst ? _T("") : (m_item.dst.bIsLocal ? _T("[Local]") : _T("[MTP]")));
    m_staticExclType.SetWindowText(!excl ? _T("") : (m_item.src.bIsLocal ? _T("[Local]") : _T("[MTP]")));
    GetDlgItem(IDC_BTN_BROWSE_EXCL)->EnableWindow(src);
    GetDlgItem(IDC_BTN_CLEAR_EXCL)->EnableWindow(excl);
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

    CMtpBrowserDlg dlg(this);
    dlg.m_bSourceMode = true;
    dlg.m_pState      = m_pSrcState;
    if (dlg.DoModal() != IDOK) return;

    CEndpoint ep;
    ep.bIsLocal      = dlg.m_bIsLocalDisk;
    ep.strDeviceId   = dlg.m_strSelectedDeviceId;
    ep.strDeviceName = DeviceName(m_pSrcState, ep.strDeviceId);
    ep.items         = dlg.m_selectedItems;

    if (ep.bIsLocal)
    {
        for (auto& it : ep.items)
            if (it.strLocalPath.IsEmpty()) it.strLocalPath = it.strObjectId;
    }
    else
    {
        // Object IDs change between connections — record the name path too.
        CWaitCursor wait;
        CWpdHelper wpd;
        if (wpd.OpenDevice(ep.strDeviceId))
            for (auto& it : ep.items)
                it.strMtpPath = wpd.GetFullPath(it.strObjectId);
    }
    ep.strDisplayPath = ListDisplay(ep, ep.items);

    // Keep the exclusions that still fall inside the new source
    bool sameDevice = (ep.strDeviceId == m_item.src.strDeviceId &&
                       ep.bIsLocal == m_item.src.bIsLocal);
    if (sameDevice) ep.excludes = m_item.src.excludes;

    m_item.src = ep;
    DropOutsideExclusions(true);
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

    // Open the browser where the source is: start from the source tree
    // (already expanded down to the source folders) unless the exclusion
    // browser is already showing that device.
    if (m_pExclState)
    {
        if (StateDeviceId(m_pExclState) != m_item.src.strDeviceId &&
            m_pSrcState && StateDeviceId(m_pSrcState) == m_item.src.strDeviceId)
        {
            *m_pExclState = *m_pSrcState;
            m_pExclState->strLastSelDisplayPath.Empty();
        }
        // Highlight the current exclusions (and nothing else)
        for (auto& snap : m_pExclState->treeSnapshot)
        {
            snap.bSelected = false;
            for (auto& x : m_item.src.excludes)
                if (!x.strObjectId.IsEmpty() && x.strObjectId == snap.item.strObjectId)
                    snap.bSelected = true;
        }
    }

    CMtpBrowserDlg dlg(this);
    dlg.m_bSourceMode = true;
    dlg.m_pState      = m_pExclState;
    dlg.m_strCaption  = _T("Select Files / Folders to Exclude");
    if (dlg.DoModal() != IDOK) return;

    if (dlg.m_strSelectedDeviceId != m_item.src.strDeviceId ||
        (dlg.m_bIsLocalDisk ? true : false) != m_item.src.bIsLocal)
    {
        CString name = m_item.src.strDeviceName.IsEmpty() ? m_item.src.strDeviceId
                                                          : m_item.src.strDeviceName;
        MessageBox(_T("Exclusions must be on the same device / drive as the source (") + name + _T(")."),
                   _T("Exclusion"), MB_ICONWARNING);
        return;
    }

    std::vector<CSourceItem> list = dlg.m_selectedItems;
    if (m_item.src.bIsLocal)
    {
        for (auto& it : list)
            if (it.strLocalPath.IsEmpty()) it.strLocalPath = it.strObjectId;
    }
    else
    {
        CWaitCursor wait;
        CWpdHelper wpd;
        if (wpd.OpenDevice(m_item.src.strDeviceId))
            for (auto& it : list)
                it.strMtpPath = wpd.GetFullPath(it.strObjectId);
    }

    m_item.src.excludes = list;            // the selection replaces the old list
    DropOutsideExclusions(true);
    UpdateUi();
}

void CItemDlg::OnBtnClearExcl()
{
    m_item.src.excludes.clear();
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
        ep.strLocalPath   = dlg.m_strSelectedLocalPath;
        ep.strDisplayPath = ep.strLocalPath;
    }
    else
    {
        ep.strObjectId = dlg.m_strSelectedObjectId;
        CWaitCursor wait;
        CWpdHelper wpd;
        if (wpd.OpenDevice(ep.strDeviceId))
            ep.strMtpPath = wpd.GetFullPath(ep.strObjectId);
        ep.strDisplayPath = ep.strMtpPath.IsEmpty()
                            ? dlg.m_strSelectedDisplayPath
                            : ep.strDeviceName + _T(" / ") + ep.strMtpPath;
    }

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
    int sel = m_comboConflict.GetCurSel();
    m_item.nOnConflict = sel >= 0 ? (int)m_comboConflict.GetItemData(sel) : CONFLICT_NEWER;
    CDialogEx::OnOK();
}
