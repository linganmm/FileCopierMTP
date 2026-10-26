#include "pch.h"
#include "PlanDlg.h"
#include "ItemDlg.h"

BEGIN_MESSAGE_MAP(CPlanDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_ADD_ITEM,   &CPlanDlg::OnBtnAddItem)
    ON_BN_CLICKED(IDC_BTN_SAVE,       &CPlanDlg::OnBtnSave)
    ON_BN_CLICKED(IDC_BTN_SAVE_AS,    &CPlanDlg::OnBtnSaveAs)
    ON_BN_CLICKED(IDC_BTN_SAVE_CLOSE, &CPlanDlg::OnBtnSaveClose)
    ON_MESSAGE(WM_ACTIONLIST_BUTTON,  &CPlanDlg::OnListButton)
    ON_MESSAGE(WM_ACTIONLIST_DBLCLK,  &CPlanDlg::OnListDblClk)
END_MESSAGE_MAP()

CPlanDlg::CPlanDlg(const CBackupPlan& plan, bool bEditMode, CWnd* pParent)
    : CDialogEx(IDD_PLAN_DIALOG, pParent)
    , m_plan(plan)
    , m_bEditMode(bEditMode)
{
    m_strDir = m_plan.strFilePath.IsEmpty() ? PlanFile::GetExeDir()
                                            : PlanFile::DirOf(m_plan.strFilePath);
}

void CPlanDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_PLAN_TITLE,   m_editTitle);
    DDX_Control(pDX, IDC_STATIC_TITLE_HINT, m_staticHint);
    DDX_Control(pDX, IDC_STATIC_PLAN_FILE,  m_staticFile);
    DDX_Control(pDX, IDC_STATIC_PLAN_TIMES, m_staticTimes);
}

BOOL CPlanDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    SetWindowText(m_bEditMode ? _T("Edit Backup Plan") : _T("New Backup Plan"));

    m_editTitle.SetLimitText(120);
    m_editTitle.SetWindowText(m_plan.strTitle);

    // Replace the placeholder frame with the item list
    CWnd* pFrame = GetDlgItem(IDC_ITEM_LIST_FRAME);
    CRect rc;
    pFrame->GetWindowRect(&rc);
    ScreenToClient(&rc);
    pFrame->ShowWindow(SW_HIDE);

    m_list.SetButtonLabels({ _T("Edit"), _T("Delete") });
    m_list.SetEmptyText(_T("No items yet.\nClick \"Add Item...\" to choose what to back up and where to."));
    m_list.Create(rc, this, IDC_ITEM_LIST);
    m_list.SetWindowPos(GetDlgItem(IDC_BTN_ADD_ITEM), 0, 0, 0, 0,
                        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);   // tab order

    for (int i = 0; i < (int)m_plan.items.size(); ++i)
        m_list.AddRow(RowTitle(i), RowSubtitle(i));

    RefreshInfo();
    return TRUE;
}

// ── Rows ──────────────────────────────────────────────────────────────────────

CString CPlanDlg::RowTitle(int index) const
{
    const CEndpoint& src = m_plan.items[index].src;
    CString s;
    s.Format(_T("%d.  %s"), index + 1, (LPCTSTR)PlanFile::DescribeEndpoint(src));
    if (!src.excludes.empty())
    {
        CString x;
        x.Format(_T("   (excluding %d)"), (int)src.excludes.size());
        s += x;
    }
    return s;
}

CString CPlanDlg::RowSubtitle(int index) const
{
    const CPlanItem& it = m_plan.items[index];
    return _T("\u2192  ") + PlanFile::DescribeEndpoint(it.dst) +
           _T("     [if file exists: ") + ConflictPolicyName(it.nOnConflict) + _T("]");
}

void CPlanDlg::RefreshRows(int fromIndex)
{
    for (int i = max(0, fromIndex); i < (int)m_plan.items.size(); ++i)
        m_list.SetRowText(i, RowTitle(i), RowSubtitle(i));
}

void CPlanDlg::RefreshInfo()
{
    std::vector<CString> dirs{ PlanFile::GetExeDir(), m_strDir };
    CString hint;
    hint.Format(_T("Optional. If left blank, the plan is saved as \"%s\"."),
                (LPCTSTR)PlanFile::MakeDefaultTitle(dirs));
    m_staticHint.SetWindowText(hint);

    if (m_plan.strFilePath.IsEmpty())
        m_staticFile.SetWindowText(_T("Not saved yet. Save writes to: ") + m_strDir);
    else
        m_staticFile.SetWindowText(_T("File: ") + m_plan.strFilePath);

    if (m_plan.strCreated.IsEmpty())
        m_staticTimes.SetWindowText(_T(""));
    else
        m_staticTimes.SetWindowText(_T("Created: ") + PlanFile::FormatForDisplay(m_plan.strCreated) +
                                    _T("      Last modified: ") +
                                    PlanFile::FormatForDisplay(m_plan.strModified));
}

// ── Items ─────────────────────────────────────────────────────────────────────

void CPlanDlg::OnBtnAddItem()
{
    CItemDlg dlg(&m_srcState, &m_exclState, &m_dstState, false, this);
    if (dlg.DoModal() != IDOK) return;

    m_plan.items.push_back(dlg.m_item);
    int i = (int)m_plan.items.size() - 1;
    m_list.AddRow(RowTitle(i), RowSubtitle(i));
    m_list.SetSel(i);
}

void CPlanDlg::EditItem(int index)
{
    if (index < 0 || index >= (int)m_plan.items.size()) return;
    CItemDlg dlg(&m_srcState, &m_exclState, &m_dstState, true, this);
    dlg.m_item = m_plan.items[index];
    if (dlg.DoModal() != IDOK) return;

    m_plan.items[index] = dlg.m_item;
    m_list.SetRowText(index, RowTitle(index), RowSubtitle(index));
}

void CPlanDlg::DeleteItem(int index)
{
    if (index < 0 || index >= (int)m_plan.items.size()) return;
    m_plan.items.erase(m_plan.items.begin() + index);
    m_list.DeleteRow(index);          // rows below move up
    RefreshRows(index);               // renumber them
}

LRESULT CPlanDlg::OnListButton(WPARAM wParam, LPARAM)
{
    int row = (int)LOWORD(wParam);
    int btn = (int)HIWORD(wParam);
    if (btn == BTN_EDIT)   EditItem(row);
    if (btn == BTN_DELETE) DeleteItem(row);
    return 0;
}

LRESULT CPlanDlg::OnListDblClk(WPARAM wParam, LPARAM)
{
    EditItem((int)wParam);
    return 0;
}

// ── Saving ────────────────────────────────────────────────────────────────────

bool CPlanDlg::SaveTo(const CString& dir)
{
    CString title;
    m_editTitle.GetWindowText(title);
    title.Trim();

    CString err;
    if (title.IsEmpty())
    {
        title = PlanFile::MakeDefaultTitle({ PlanFile::GetExeDir(), dir });
        m_editTitle.SetWindowText(title);
    }
    else if (!PlanFile::ValidateTitle(title, err))
    {
        MessageBox(err, _T("Invalid title"), MB_ICONWARNING);
        m_editTitle.SetFocus();
        m_editTitle.SetSel(0, -1);
        return false;
    }

    CString path = PlanFile::BuildPath(dir, title);
    bool sameFile = !m_plan.strFilePath.IsEmpty() &&
                    path.CompareNoCase(m_plan.strFilePath) == 0;
    if (!sameFile && ::GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
    {
        CString q;
        q.Format(_T("A backup plan named \"%s\" already exists in\n%s\n\nReplace it?"),
                 (LPCTSTR)title, (LPCTSTR)dir);
        if (MessageBox(q, _T("Save Backup Plan"), MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
            return false;
    }

    CBackupPlan p = m_plan;
    p.strTitle = title;
    CString now = PlanFile::NowIso8601();
    if (p.strCreated.IsEmpty()) p.strCreated = now;
    p.strModified = now;

    if (!PlanFile::Save(p, path, err))
    {
        MessageBox(err, _T("Save Backup Plan"), MB_ICONERROR);
        return false;
    }

    // Changing the title of a saved plan renames its file (in the same
    // folder).  Save As to another folder leaves the original as a copy.
    if (!m_plan.strFilePath.IsEmpty() && !sameFile &&
        PlanFile::DirOf(m_plan.strFilePath).CompareNoCase(dir) == 0)
    {
        ::DeleteFile(m_plan.strFilePath);
    }

    p.strFilePath = path;
    m_plan      = p;
    m_strDir    = dir;
    m_bSaved    = true;
    m_savedPlan = p;
    RefreshInfo();
    return true;
}

void CPlanDlg::OnBtnSave()
{
    SaveTo(m_strDir);
}

void CPlanDlg::OnBtnSaveAs()
{
    CFolderPickerDialog dlg(m_strDir, 0, this);
    dlg.m_ofn.lpstrTitle = _T("Select a folder to save the backup plan in");
    if (dlg.DoModal() != IDOK) return;

    CString dir = dlg.GetPathName();
    if (dir.IsEmpty()) return;
    SaveTo(dir);
}

void CPlanDlg::OnBtnSaveClose()
{
    if (SaveTo(m_strDir))
        EndDialog(IDOK);
}

void CPlanDlg::OnOK()
{
    // Enter is routed to the default button (Save & Close); a stray IDOK
    // must not close the dialog without saving.
}

void CPlanDlg::OnCancel()
{
    EndDialog(m_bSaved ? IDOK : IDCANCEL);
}
