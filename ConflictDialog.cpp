#include "pch.h"
#include "ConflictDialog.h"

IMPLEMENT_DYNAMIC(CConflictDialog, CDialogEx)

BEGIN_MESSAGE_MAP(CConflictDialog, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_OVERWRITE, &CConflictDialog::OnBtnOverwrite)
    ON_BN_CLICKED(IDC_BTN_SKIP,      &CConflictDialog::OnBtnSkip)
    ON_BN_CLICKED(IDC_BTN_NEWER, &CConflictDialog::OnBtnNewer)
    ON_BN_CLICKED(IDC_BTN_CANCEL_OP, &CConflictDialog::OnBtnCancel)
END_MESSAGE_MAP()

CConflictDialog::CConflictDialog(const CConflictInfo& info, CWnd* pParent)
    : CDialogEx(IDD_CONFLICT_DIALOG, pParent)
    , m_info(info), m_nResolution(CONFLICT_SKIP), m_bApplyToAll(FALSE)
{}
CConflictDialog::~CConflictDialog() {}

void CConflictDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_CHK_APPLY_ALL,      m_chkApplyAll);
    DDX_Control(pDX, IDC_STATIC_CONFLICT_MSG, m_staticMsg);
}

// ── FormatFileTime ─────────────────────────────────────────────────────────

CString CConflictDialog::FormatFileTime(const FILETIME& ft)
{
    if (ft.dwLowDateTime == 0 && ft.dwHighDateTime == 0)
        return _T("(unknown)");

    FILETIME ftLocal;
    SYSTEMTIME st;
    if (!FileTimeToLocalFileTime(&ft, &ftLocal) ||
        !FileTimeToSystemTime(&ftLocal, &st))
        return _T("(invalid)");

    CString s;
    s.Format(_T("%04d-%02d-%02d  %02d:%02d:%02d"),
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond);
    return s;
}

BOOL CConflictDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    CString srcTime = FormatFileTime(m_info.ftSrc);
    CString dstTime = FormatFileTime(m_info.ftDst);

    CString msg;
    msg.Format(
        _T("The file already exists at the destination:\n\n")
        _T("\"%s\"\n\n")
        _T("Source modified:       %s\n")
        _T("Destination modified:  %s\n\n")
        _T("What would you like to do?"),
        (LPCTSTR)m_info.strRelPath,
        (LPCTSTR)srcTime,
        (LPCTSTR)dstTime);

    m_staticMsg.SetWindowText(msg);
    if (!m_info.strCaption.IsEmpty())
        SetWindowText(m_info.strCaption);
    return TRUE;
}

void CConflictDialog::OnBtnOverwrite()
{
    m_bApplyToAll = (m_chkApplyAll.GetCheck() == BST_CHECKED);
    m_nResolution = CONFLICT_OVERWRITE;
    EndDialog(IDOK);
}

void CConflictDialog::OnBtnSkip()
{
    m_bApplyToAll = (m_chkApplyAll.GetCheck() == BST_CHECKED);
    m_nResolution = CONFLICT_SKIP;
    EndDialog(IDOK);
}

void CConflictDialog::OnBtnNewer()
{
    m_bApplyToAll = (m_chkApplyAll.GetCheck() == BST_CHECKED);
    m_nResolution = CONFLICT_NEWER;
    EndDialog(IDOK);
}

void CConflictDialog::OnBtnCancel()
{
    m_nResolution = CONFLICT_CANCEL;
    EndDialog(IDCANCEL);
}
