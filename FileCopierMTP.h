#pragma once
#include "pch.h"
#include "resource.h"

// LXFreeBack application.  Starts the SDI "LXFreeBack" window; the original
// Universal File Copier dialog is still available from File > Universal File
// Copier.
class CFileCopierMTPApp : public CWinApp
{
public:
    CFileCopierMTPApp();
    virtual BOOL InitInstance();
    virtual int  ExitInstance();

    afx_msg void OnAppAbout();
    DECLARE_MESSAGE_MAP()

private:
    bool m_bComInitialized = false;
};
