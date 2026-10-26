#include "pch.h"
#include "FileCopierMTP.h"
#include "MainFrm.h"
#include "LXFreeBackDoc.h"
#include "LXFreeBackView.h"

// ── About box ────────────────────────────────────────────────────────────────

class CAboutDlg : public CDialogEx
{
public:
    CAboutDlg() : CDialogEx(IDD_ABOUTBOX) {}

protected:
    virtual BOOL OnInitDialog()
    {
        CDialogEx::OnInitDialog();
        SetDlgItemText(IDC_STATIC_ABOUT_COPYRIGHT,
            _T("Copyright \u00a9 2026 Luoxuan Intelligent Technology Co., Ltd"));
        return TRUE;
    }
};

BEGIN_MESSAGE_MAP(CFileCopierMTPApp, CWinApp)
    ON_COMMAND(ID_APP_ABOUT, &CFileCopierMTPApp::OnAppAbout)
END_MESSAGE_MAP()

void CFileCopierMTPApp::OnAppAbout()
{
    CAboutDlg dlg;
    dlg.DoModal();
}

CFileCopierMTPApp theApp;

CFileCopierMTPApp::CFileCopierMTPApp() {}

BOOL CFileCopierMTPApp::InitInstance()
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES };
    ::InitCommonControlsEx(&icc);

    CWinApp::InitInstance();
    m_bComInitialized = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));

    // SDI document template: frame "LXFreeBack" + plan list view
    CSingleDocTemplate* pDocTemplate = new CSingleDocTemplate(
        IDR_MAINFRAME,
        RUNTIME_CLASS(CLXFreeBackDoc),
        RUNTIME_CLASS(CMainFrame),
        RUNTIME_CLASS(CLXFreeBackView));
    if (!pDocTemplate)
        return FALSE;
    AddDocTemplate(pDocTemplate);

    // A .lbp path on the command line (e.g. "LXFreeBack.exe MyPlan.lbp") is
    // loaded into the list; the window itself always starts empty.
    CCommandLineInfo cmdInfo;
    ParseCommandLine(cmdInfo);
    CString planToLoad;
    if (cmdInfo.m_nShellCommand == CCommandLineInfo::FileOpen)
        planToLoad = cmdInfo.m_strFileName;
    cmdInfo.m_nShellCommand = CCommandLineInfo::FileNew;
    cmdInfo.m_strFileName.Empty();

    if (!ProcessShellCommand(cmdInfo))
        return FALSE;

    m_pMainWnd->SetWindowText(_T("LXFreeBack"));
    m_pMainWnd->ShowWindow(SW_SHOW);
    m_pMainWnd->UpdateWindow();

    if (!planToLoad.IsEmpty())
    {
        auto* pFrame = DYNAMIC_DOWNCAST(CFrameWnd, m_pMainWnd);
        auto* pView  = pFrame ? DYNAMIC_DOWNCAST(CLXFreeBackView, pFrame->GetActiveView()) : nullptr;
        if (pView) pView->LoadPlanFile(planToLoad, true);
    }
    return TRUE;
}

int CFileCopierMTPApp::ExitInstance()
{
    if (m_bComInitialized) CoUninitialize();
    return CWinApp::ExitInstance();
}
