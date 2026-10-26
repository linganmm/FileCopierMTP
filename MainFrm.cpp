#include "pch.h"
#include "resource.h"
#include "MainFrm.h"
#include "LXFreeBackView.h"

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_CLOSE()
    ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()

static UINT indicators[] =
{
    ID_SEPARATOR,           // status line indicator
};

CMainFrame::CMainFrame() {}
CMainFrame::~CMainFrame() {}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CFrameWnd::PreCreateWindow(cs))
        return FALSE;

    // Title is just "LXFreeBack" (no document name)
    cs.style &= ~FWS_ADDTOTITLE;

    // A sensible initial size, centred on the primary work area
    CRect work;
    ::SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0);
    HDC hdc = ::GetDC(nullptr);
    int dpi = ::GetDeviceCaps(hdc, LOGPIXELSX);
    ::ReleaseDC(nullptr, hdc);
    cs.cx = min(work.Width(),  ::MulDiv(820, dpi, 96));
    cs.cy = min(work.Height(), ::MulDiv(520, dpi, 96));
    cs.x  = work.left + (work.Width()  - cs.cx) / 2;
    cs.y  = work.top  + (work.Height() - cs.cy) / 2;
    return TRUE;
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
        return -1;

    if (!m_wndStatusBar.Create(this) ||
        !m_wndStatusBar.SetIndicators(indicators, sizeof(indicators) / sizeof(UINT)))
    {
        TRACE0("Failed to create status bar\n");
        return -1;
    }
    SetWindowText(_T("LXFreeBack"));
    return 0;
}

void CMainFrame::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = 520;
    lpMMI->ptMinTrackSize.y = 260;
    CFrameWnd::OnGetMinMaxInfo(lpMMI);
}

void CMainFrame::OnClose()
{
    CLXFreeBackView* pView = DYNAMIC_DOWNCAST(CLXFreeBackView, GetActiveView());
    if (pView && !pView->PrepareToClose())
        return;
    CFrameWnd::OnClose();
}
