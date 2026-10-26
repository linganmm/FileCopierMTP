#include "pch.h"
#include "MtpBrowserDlg.h"
#include <filesystem>
namespace fs = std::filesystem;

IMPLEMENT_DYNAMIC(CMtpBrowserDlg, CDialogEx)
const TCHAR* CMtpBrowserDlg::kDummyId = _T("__DUMMY__");

BEGIN_MESSAGE_MAP(CMtpBrowserDlg, CDialogEx)
    ON_CBN_SELCHANGE(IDC_COMBO_DEVICE,         &CMtpBrowserDlg::OnCbnSelchangeDevice)
    ON_NOTIFY(TVN_ITEMEXPANDING, IDC_TREE_MTP, &CMtpBrowserDlg::OnTvnItemExpandingTree)
    ON_NOTIFY(TVN_SELCHANGED,    IDC_TREE_MTP, &CMtpBrowserDlg::OnTvnSelchangedTree)
    ON_NOTIFY(NM_CLICK,          IDC_TREE_MTP, &CMtpBrowserDlg::OnNmClickTree)
    ON_BN_CLICKED(IDC_BTN_OK,     &CMtpBrowserDlg::OnOK)
    ON_BN_CLICKED(IDC_BTN_CANCEL, &CMtpBrowserDlg::OnCancel)
END_MESSAGE_MAP()

CMtpBrowserDlg::CMtpBrowserDlg(CWnd* pParent)
    : CDialogEx(IDD_BROWSE_MTP_DIALOG, pParent)
    , m_pWpd(std::make_unique<CWpdHelper>())
{}

CMtpBrowserDlg::~CMtpBrowserDlg() { ClearItemData(); }

void CMtpBrowserDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_COMBO_DEVICE,     m_comboDevice);
    DDX_Control(pDX, IDC_TREE_MTP,         m_treeCtrl);
    DDX_Control(pDX, IDC_STATIC_SEL_PATH,  m_staticSelPath);
    DDX_Control(pDX, IDC_STATIC_SEL_COUNT, m_staticSelCount);
    DDX_Control(pDX, IDC_BTN_OK,           m_btnOk);
    DDX_Control(pDX, IDC_BTN_CANCEL,       m_btnCancel);
}

void CMtpBrowserDlg::ClearItemData()
{
    for (auto* p : m_itemData) delete p;
    m_itemData.clear();
    m_multiSel.clear();
}

// ── Highlight helpers ─────────────────────────────────────────────────────────
// Selected items are shown bold; the normal blue highlight tracks last click.

void CMtpBrowserDlg::SetItemHighlight(HTREEITEM hItem, bool bHighlight)
{
    TVITEM tvi    = {};
    tvi.hItem     = hItem;
    tvi.mask      = TVIF_STATE;
    tvi.stateMask = TVIS_BOLD;
    tvi.state     = bHighlight ? TVIS_BOLD : 0;
    m_treeCtrl.SetItem(&tvi);
}

bool CMtpBrowserDlg::IsItemSelected(HTREEITEM hItem) const
{
    auto* pSI = reinterpret_cast<CSourceItem*>(m_treeCtrl.GetItemData(hItem));
    return pSI && m_multiSel.count(pSI) > 0;
}

// ── OnInitDialog ──────────────────────────────────────────────────────────────

BOOL CMtpBrowserDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    if (m_bSourceMode)
    {
        SetWindowText(_T("Select Source Files / Folders"));
        AdjustSourceDialog();
    }
    else
    {
        SetWindowText(_T("Select Destination Folder"));
        AdjustDestDialog();
    }

    if (!m_strCaption.IsEmpty())
        SetWindowText(m_strCaption);

    m_btnOk.EnableWindow(FALSE);

    // ── Restore from saved state ──────────────────────────────────────────────
    if (m_pState && m_pState->bInitialized && !m_pState->devices.empty())
    {
        m_devices = m_pState->devices;
        for (auto& d : m_devices)
            m_comboDevice.AddString(d.strFriendlyName);
        m_comboDevice.SetCurSel(m_pState->nLastDeviceIdx);

        int idx = m_pState->nLastDeviceIdx;
        if (idx >= 0 && idx < (int)m_devices.size())
        {
            m_pWpd->OpenDevice(m_devices[idx].strDeviceId);
            m_bIsLocalDisk        = m_devices[idx].bIsLocalDisk;
            m_strSelectedDeviceId = m_devices[idx].strDeviceId;
        }

        RestoreTree();

        if (!m_pState->strLastSelDisplayPath.IsEmpty())
        {
            m_staticSelPath.SetWindowText(m_pState->strLastSelDisplayPath);
            m_strSelectedDisplayPath = m_pState->strLastSelDisplayPath;
            m_btnOk.EnableWindow(TRUE);
        }
        UpdateSelectionCount();
        return TRUE;
    }

    // ── First open ────────────────────────────────────────────────────────────
    m_devices = CWpdHelper::EnumerateDevices();
    if (m_pState) m_pState->devices = m_devices;

    if (m_devices.empty())
    {
        m_comboDevice.AddString(_T("(No devices or drives found)"));
        m_comboDevice.SetCurSel(0);
        m_comboDevice.EnableWindow(FALSE);
        if (m_pState) m_pState->bInitialized = true;
        return TRUE;
    }

    for (auto& d : m_devices)
        m_comboDevice.AddString(d.strFriendlyName);
    m_comboDevice.SetCurSel(0);
    LoadDevice(0);

    if (m_pState) { m_pState->bInitialized = true; m_pState->nLastDeviceIdx = 0; }
    return TRUE;
}

// ── AdjustSourceDialog / AdjustDestDialog ────────────────────────────────────
// Preserved from the uploaded package — dynamically resize the dialog based
// on mode. Source mode hides the "Selected path:" row and shrinks the dialog.
// Destination mode hides the count label and shows "Selected path:".

void CMtpBrowserDlg::AdjustSourceDialog()
{
    // Source mode: no checkboxes — Ctrl+click instead
    m_staticSelCount.ShowWindow(SW_SHOW);
    m_staticSelPath.ShowWindow(SW_HIDE);
    GetDlgItem(IDC_STATIC_SEL_PATH_LABEL)->ShowWindow(SW_HIDE);

    // Move OK/Cancel up into the space where "Selected path:" was
    CRect rcBtnOK, rcBtnCancel, rcSelPath, rcBrowseDlg;
    GetDlgItem(IDC_STATIC_SEL_PATH)->GetWindowRect(&rcSelPath);
    ScreenToClient(&rcSelPath);

    GetDlgItem(IDC_BTN_OK)->GetWindowRect(&rcBtnOK);
    ScreenToClient(&rcBtnOK);
    m_btnOk.MoveWindow(rcBtnOK.left, rcSelPath.top, rcBtnOK.Width(), rcBtnOK.Height());

    GetDlgItem(IDC_BTN_CANCEL)->GetWindowRect(&rcBtnCancel);
    ScreenToClient(&rcBtnCancel);
    m_btnCancel.MoveWindow(rcBtnCancel.left, rcSelPath.top, rcBtnCancel.Width(), rcBtnCancel.Height());

    GetWindowRect(&rcBrowseDlg);
    MoveWindow(rcBrowseDlg.left, rcBrowseDlg.top,
               rcBrowseDlg.Width(),
               rcBrowseDlg.Height() - (rcBtnCancel.top - rcSelPath.top));
}

void CMtpBrowserDlg::AdjustDestDialog()
{
    m_staticSelCount.ShowWindow(SW_HIDE);

    // Destination mode: move "Selected path:" and buttons up into the space
    // vacated by the hidden count label
    CRect rcBtnOK, rcBtnCancel, rcSelPath, rcSelPathLabel, rcCount, rcBrowseDlg;
    GetDlgItem(IDC_STATIC_SEL_COUNT)->GetWindowRect(&rcCount);
    ScreenToClient(&rcCount);

    GetDlgItem(IDC_STATIC_SEL_PATH)->GetWindowRect(&rcSelPath);
    ScreenToClient(&rcSelPath);
    m_staticSelPath.MoveWindow(rcSelPath.left, rcCount.top, rcSelPath.Width(), rcSelPath.Height());

    GetDlgItem(IDC_STATIC_SEL_PATH_LABEL)->GetWindowRect(&rcSelPathLabel);
    ScreenToClient(&rcSelPathLabel);
    GetDlgItem(IDC_STATIC_SEL_PATH_LABEL)->MoveWindow(
        rcSelPathLabel.left, rcCount.top, rcSelPathLabel.Width(), rcSelPathLabel.Height());

    int shift = rcSelPath.top - rcCount.top;

    GetDlgItem(IDC_BTN_OK)->GetWindowRect(&rcBtnOK);
    ScreenToClient(&rcBtnOK);
    m_btnOk.MoveWindow(rcBtnOK.left, rcBtnOK.top - shift, rcBtnOK.Width(), rcBtnOK.Height());

    GetDlgItem(IDC_BTN_CANCEL)->GetWindowRect(&rcBtnCancel);
    ScreenToClient(&rcBtnCancel);
    m_btnCancel.MoveWindow(rcBtnCancel.left, rcBtnCancel.top - shift,
                           rcBtnCancel.Width(), rcBtnCancel.Height());

    GetWindowRect(&rcBrowseDlg);
    MoveWindow(rcBrowseDlg.left, rcBrowseDlg.top,
               rcBrowseDlg.Width(), rcBrowseDlg.Height() - shift);
}

// ── SaveState / RestoreTree ───────────────────────────────────────────────────

void CMtpBrowserDlg::SaveState()
{
    if (!m_pState) return;
    m_pState->nLastDeviceIdx        = max(0, m_comboDevice.GetCurSel());
    m_pState->strLastSelDisplayPath = m_strSelectedDisplayPath;
    m_pState->devices               = m_devices;
    m_pState->treeSnapshot.clear();
    SaveTreeNode(m_treeCtrl.GetRootItem(), 0, m_pState->treeSnapshot);
}

void CMtpBrowserDlg::SaveTreeNode(HTREEITEM hItem, int depth,
                                   std::vector<TreeNodeSnapshot>& out)
{
    while (hItem)
    {
        auto* pSI = reinterpret_cast<CSourceItem*>(m_treeCtrl.GetItemData(hItem));
        if (!pSI || pSI->strObjectId == kDummyId)
        {
            hItem = m_treeCtrl.GetNextSiblingItem(hItem);
            continue;
        }

        TreeNodeSnapshot snap;
        snap.depth     = depth;
        snap.item      = *pSI;
        snap.bExpanded = (m_treeCtrl.GetItemState(hItem, TVIS_EXPANDED) & TVIS_EXPANDED) != 0;
        snap.bSelected = m_bSourceMode && (m_multiSel.count(pSI) > 0);

        HTREEITEM hChild = m_treeCtrl.GetChildItem(hItem);
        snap.bHasDummy   = false;
        if (hChild)
        {
            auto* pCSI = reinterpret_cast<CSourceItem*>(m_treeCtrl.GetItemData(hChild));
            if (pCSI && pCSI->strObjectId == kDummyId)
                snap.bHasDummy = true;
        }
        out.push_back(snap);

        if (hChild && !snap.bHasDummy)
            SaveTreeNode(hChild, depth + 1, out);

        hItem = m_treeCtrl.GetNextSiblingItem(hItem);
    }
}

void CMtpBrowserDlg::RestoreTree()
{
    m_treeCtrl.DeleteAllItems();
    ClearItemData();
    if (!m_pState || m_pState->treeSnapshot.empty()) return;

    std::vector<HTREEITEM> parentStack(32, TVI_ROOT);
    std::vector<HTREEITEM> toExpand;
    std::vector<HTREEITEM> toSelect;

    // Pass 1: insert all nodes
    for (auto& snap : m_pState->treeSnapshot)
    {
        HTREEITEM hParent = (snap.depth == 0) ? TVI_ROOT : parentStack[snap.depth];

        TVINSERTSTRUCT tvis = {};
        tvis.hParent        = hParent;
        tvis.hInsertAfter   = TVI_LAST;
        tvis.item.mask      = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN;
        tvis.item.pszText   = const_cast<LPTSTR>((LPCTSTR)snap.item.strName);
        tvis.item.cChildren = snap.item.bIsFolder ? 1 : 0;

        auto* si = new CSourceItem(snap.item);
        m_itemData.push_back(si);
        tvis.item.lParam = (LPARAM)si;

        HTREEITEM hItem = m_treeCtrl.InsertItem(&tvis);
        if (snap.depth + 1 < (int)parentStack.size())
            parentStack[snap.depth + 1] = hItem;

        if (snap.item.bIsFolder && snap.bHasDummy)
        {
            TVINSERTSTRUCT dummy = {};
            dummy.hParent        = hItem;
            dummy.hInsertAfter   = TVI_LAST;
            dummy.item.mask      = TVIF_TEXT | TVIF_PARAM;
            dummy.item.pszText   = const_cast<LPTSTR>(kDummyId);
            auto* dsi            = new CSourceItem();
            dsi->strObjectId     = kDummyId;
            m_itemData.push_back(dsi);
            dummy.item.lParam    = (LPARAM)dsi;
            m_treeCtrl.InsertItem(&dummy);
        }

        if (snap.bExpanded && !snap.bHasDummy) toExpand.push_back(hItem);
        if (m_bSourceMode && snap.bSelected)   toSelect.push_back(hItem);
    }

    // Pass 2: expand deepest-first
    for (int i = (int)toExpand.size() - 1; i >= 0; --i)
        m_treeCtrl.Expand(toExpand[i], TVE_EXPAND);

    // Pass 3: restore multi-selection highlight
    for (HTREEITEM h : toSelect)
    {
        auto* pSI = reinterpret_cast<CSourceItem*>(m_treeCtrl.GetItemData(h));
        if (pSI) { m_multiSel.insert(pSI); SetItemHighlight(h, true); }
    }

    if (!toSelect.empty()) m_btnOk.EnableWindow(TRUE);
    UpdateSelectionCount();
}

// ── OnOK / OnCancel ───────────────────────────────────────────────────────────

void CMtpBrowserDlg::OnOK()
{
    if (m_bSourceMode)
    {
        m_selectedItems.clear();
        CollectSelected(m_treeCtrl.GetRootItem());
        if (m_selectedItems.empty())
        {
            MessageBox(_T("Please Ctrl+Click at least one file or folder."),
                       _T("No selection"), MB_ICONINFORMATION);
            return;
        }
        CString disp;
        int n = (int)m_selectedItems.size();
        for (int i = 0; i < min(n, 3); ++i)
        {
            if (!disp.IsEmpty()) disp += _T(", ");
            disp += m_selectedItems[i].strName;
        }
        if (n > 3) { CString s; s.Format(_T(" (+%d more)"), n - 3); disp += s; }
        m_strSelectedDisplayPath = disp;
    }
    else
    {
        bool ok = m_bIsLocalDisk ? !m_strSelectedLocalPath.IsEmpty()
                                 : !m_strSelectedObjectId.IsEmpty();
        if (!ok)
        {
            MessageBox(_T("Please select a destination folder."),
                       _T("No selection"), MB_ICONINFORMATION);
            return;
        }
    }
    SaveState();
    CDialogEx::OnOK();
}

void CMtpBrowserDlg::OnCancel()
{
    SaveState();
    CDialogEx::OnCancel();
}

// ── Device selection ──────────────────────────────────────────────────────────

void CMtpBrowserDlg::OnCbnSelchangeDevice()
{
    int idx = m_comboDevice.GetCurSel();
    if (idx >= 0 && idx < (int)m_devices.size())
        LoadDevice(idx);
}

void CMtpBrowserDlg::LoadDevice(int idx)
{
    m_treeCtrl.DeleteAllItems();
    ClearItemData();
    m_strSelectedObjectId.Empty();
    m_strSelectedLocalPath.Empty();
    m_strSelectedDisplayPath.Empty();
    m_staticSelPath.SetWindowText(_T(""));
    m_btnOk.EnableWindow(FALSE);
    UpdateSelectionCount();

    const MtpDeviceInfo& dev = m_devices[idx];
    m_pWpd->CloseDevice();
    SetCursor(LoadCursor(nullptr, IDC_WAIT));
    bool ok = m_pWpd->OpenDevice(dev.strDeviceId);
    SetCursor(LoadCursor(nullptr, IDC_ARROW));

    if (!ok)
    {
        MessageBox(
            _T("Failed to open device/drive.\n")
            _T("For Android: unlock the phone and select 'File Transfer' mode."),
            _T("Error"), MB_ICONWARNING);
        return;
    }

    m_bIsLocalDisk        = dev.bIsLocalDisk;
    m_strSelectedDeviceId = dev.strDeviceId;

    CString rootId = dev.bIsLocalDisk ? dev.strDeviceId : _T("");
    PopulateTreeChildren(TVI_ROOT, rootId, m_bSourceMode);
}

// ── Tree population ───────────────────────────────────────────────────────────

void CMtpBrowserDlg::PopulateTreeChildren(HTREEITEM hParent,
                                           const CString& strParentId,
                                           bool bFilesToo)
{
    auto children = m_pWpd->GetChildren(strParentId);
    bool anyAdded = false;

    for (auto& node : children)
    {
        if (!m_bSourceMode && !node.bIsFolder) continue;

        TVINSERTSTRUCT tvis = {};
        tvis.hParent        = hParent;
        tvis.hInsertAfter   = TVI_SORT;
        tvis.item.mask      = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN;
        tvis.item.pszText   = const_cast<LPTSTR>((LPCTSTR)node.strName);
        tvis.item.cChildren = node.bIsFolder ? 1 : 0;

        auto* si         = new CSourceItem();
        si->bIsFolder    = node.bIsFolder;
        si->strObjectId  = node.strObjectId;
        si->strName      = node.strName;
        si->strLocalPath = node.strObjectId;
        m_itemData.push_back(si);
        tvis.item.lParam = (LPARAM)si;

        HTREEITEM hItem = m_treeCtrl.InsertItem(&tvis);

        if (node.bIsFolder)
        {
            TVINSERTSTRUCT dummy = {};
            dummy.hParent        = hItem;
            dummy.hInsertAfter   = TVI_LAST;
            dummy.item.mask      = TVIF_TEXT | TVIF_PARAM;
            dummy.item.pszText   = const_cast<LPTSTR>(kDummyId);
            auto* dsi            = new CSourceItem();
            dsi->strObjectId     = kDummyId;
            m_itemData.push_back(dsi);
            dummy.item.lParam    = (LPARAM)dsi;
            m_treeCtrl.InsertItem(&dummy);
        }
        anyAdded = true;
    }

    if (!anyAdded && hParent != TVI_ROOT)
    {
        TVITEM tvi    = {};
        tvi.hItem     = hParent;
        tvi.mask      = TVIF_CHILDREN;
        tvi.cChildren = 0;
        m_treeCtrl.SetItem(&tvi);
    }
}

void CMtpBrowserDlg::OnTvnItemExpandingTree(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMTREEVIEW pNMTV = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
    *pResult = 0;
    if (pNMTV->action != TVE_EXPAND) return;

    HTREEITEM hItem  = pNMTV->itemNew.hItem;
    HTREEITEM hChild = m_treeCtrl.GetChildItem(hItem);
    if (!hChild) return;

    auto* pSI = reinterpret_cast<CSourceItem*>(m_treeCtrl.GetItemData(hChild));
    if (!pSI || pSI->strObjectId != kDummyId) return;

    m_treeCtrl.DeleteItem(hChild);

    auto* pParentSI = reinterpret_cast<CSourceItem*>(pNMTV->itemNew.lParam);
    if (pParentSI && pParentSI->strObjectId != kDummyId)
    {
        SetCursor(LoadCursor(nullptr, IDC_WAIT));
        PopulateTreeChildren(hItem, pParentSI->strObjectId, m_bSourceMode);
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
    }
}

// ── Single-click selection (destination mode) ─────────────────────────────────

void CMtpBrowserDlg::OnTvnSelchangedTree(NMHDR* pNMHDR, LRESULT* pResult)
{
    *pResult = 0;
    if (m_bSourceMode) return;

    LPNMTREEVIEW pNMTV = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
    HTREEITEM hItem    = pNMTV->itemNew.hItem;
    if (!hItem) return;

    auto* pSI = reinterpret_cast<CSourceItem*>(pNMTV->itemNew.lParam);
    if (!pSI || pSI->strObjectId == kDummyId || !pSI->bIsFolder) return;

    int devIdx = m_comboDevice.GetCurSel();
    if (devIdx < 0 || devIdx >= (int)m_devices.size()) return;

    if (m_bIsLocalDisk)
        m_strSelectedLocalPath = pSI->strLocalPath;
    else
        m_strSelectedObjectId  = pSI->strObjectId;

    m_strSelectedDisplayPath = m_devices[devIdx].strFriendlyName
                               + _T(" / ") + BuildDisplayPath(hItem);
    m_staticSelPath.SetWindowText(m_strSelectedDisplayPath);
    m_btnOk.EnableWindow(TRUE);
}

// ── Ctrl+Click multi-select (source mode) ────────────────────────────────────
//
// NM_CLICK fires after the tree moves the single selection to the clicked item.
//   Ctrl held  → toggle this item in/out of m_multiSel, preserve the rest
//   No Ctrl    → clear m_multiSel, select only this item
// Visual: selected items are bold (TVIS_BOLD).

void CMtpBrowserDlg::OnNmClickTree(NMHDR* pNMHDR, LRESULT* pResult)
{
    *pResult = 0;
    if (!m_bSourceMode) return;

    DWORD pos = GetMessagePos();
    CPoint pt(GET_X_LPARAM(pos), GET_Y_LPARAM(pos));
    m_treeCtrl.ScreenToClient(&pt);

    TVHITTESTINFO ht = {};
    ht.pt = pt;
    HTREEITEM hItem = m_treeCtrl.HitTest(&ht);
    if (!hItem) return;
    if (!(ht.flags & (TVHT_ONITEM | TVHT_ONITEMBUTTON))) return;

    auto* pSI = reinterpret_cast<CSourceItem*>(m_treeCtrl.GetItemData(hItem));
    if (!pSI || pSI->strObjectId == kDummyId) return;

    bool ctrlHeld = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    if (ctrlHeld)
    {
        // Toggle this item
        if (m_multiSel.count(pSI))
        {
            m_multiSel.erase(pSI);
            SetItemHighlight(hItem, false);
        }
        else
        {
            m_multiSel.insert(pSI);
            SetItemHighlight(hItem, true);
        }
    }
    else
    {
        // Clear all, select only this item
        for (HTREEITEM h = m_treeCtrl.GetRootItem(); h;
             h = m_treeCtrl.GetNextVisibleItem(h))
        {
            if (IsItemSelected(h))
                SetItemHighlight(h, false);
        }
        m_multiSel.clear();
        m_multiSel.insert(pSI);
        SetItemHighlight(hItem, true);
    }

    UpdateSelectionCount();
    m_btnOk.EnableWindow(!m_multiSel.empty());
}

// ── Collect / count ───────────────────────────────────────────────────────────

void CMtpBrowserDlg::CollectSelected(HTREEITEM hItem)
{
    while (hItem)
    {
        auto* pSI = reinterpret_cast<CSourceItem*>(m_treeCtrl.GetItemData(hItem));
        if (pSI && pSI->strObjectId != kDummyId && m_multiSel.count(pSI))
        {
            m_selectedItems.push_back(*pSI);
            // Don't recurse into selected folders — copy engine expands them
        }
        else
        {
            HTREEITEM hChild = m_treeCtrl.GetChildItem(hItem);
            if (hChild) CollectSelected(hChild);
        }
        hItem = m_treeCtrl.GetNextSiblingItem(hItem);
    }
}

void CMtpBrowserDlg::UpdateSelectionCount()
{
    if (!m_bSourceMode || !m_staticSelCount.GetSafeHwnd()) return;
    CString msg;
    msg.Format(_T("%d item(s) selected"), (int)m_multiSel.size());
    m_staticSelCount.SetWindowText(msg);
}

CString CMtpBrowserDlg::BuildDisplayPath(HTREEITEM hItem)
{
    CString path;
    while (hItem)
    {
        TCHAR buf[512] = {};
        TVITEM tvi     = {};
        tvi.hItem      = hItem;
        tvi.mask       = TVIF_TEXT;
        tvi.pszText    = buf;
        tvi.cchTextMax = 511;
        m_treeCtrl.GetItem(&tvi);
        path  = CString(buf) + (path.IsEmpty() ? _T("") : _T("/") + path);
        hItem = m_treeCtrl.GetParentItem(hItem);
    }
    return path;
}
