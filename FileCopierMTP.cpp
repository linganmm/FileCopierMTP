#include "pch.h"
#include "FileCopierMTP.h"
#include "MainDlg.h"

BEGIN_MESSAGE_MAP(CFileCopierMTPApp, CWinApp)
END_MESSAGE_MAP()

CFileCopierMTPApp theApp;

CFileCopierMTPApp::CFileCopierMTPApp() {}

BOOL CFileCopierMTPApp::InitInstance()
{
    CWinApp::InitInstance();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    CMainDlg dlg;
    m_pMainWnd = &dlg;
    dlg.DoModal();

    CoUninitialize();
    return FALSE;
}
