#include "pch.h"
#include "ActionList.h"

BEGIN_MESSAGE_MAP(CActionList, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_VSCROLL()
    ON_WM_MOUSEWHEEL()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_KEYDOWN()
    ON_WM_GETDLGCODE()
    ON_WM_SETFOCUS()
    ON_WM_KILLFOCUS()
    ON_WM_SYSCOLORCHANGE()
END_MESSAGE_MAP()

CActionList::Row::~Row()
{
    // Destroy explicitly so CWnd's destructor doesn't have to (avoids the
    // "calling DestroyWindow in CWnd::~CWnd" trace in debug builds).
    for (auto& b : buttons)
        if (b && b->GetSafeHwnd()) b->DestroyWindow();
    if (progress && progress->GetSafeHwnd()) progress->DestroyWindow();
}

CActionList::CActionList() {}

CActionList::~CActionList()
{
    m_rows.clear();
}

BOOL CActionList::Create(const CRect& rc, CWnd* pParent, UINT nID)
{
    CString cls = AfxRegisterWndClass(CS_DBLCLKS,
                                      ::LoadCursor(nullptr, IDC_ARROW),
                                      nullptr, nullptr);
    InitFontsAndMetrics(pParent);
    BOOL ok = CWnd::CreateEx(WS_EX_CLIENTEDGE | WS_EX_CONTROLPARENT, cls, _T(""),
                             WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP |
                             WS_CLIPCHILDREN,
                             rc, pParent, nID);
    if (ok) UpdateScrollBar();
    return ok;
}

void CActionList::InitFontsAndMetrics(CWnd* pParent)
{
    LOGFONT lf = {};
    CFont* pParentFont = pParent ? pParent->GetFont() : nullptr;
    if (pParentFont && pParentFont->GetLogFont(&lf))
    {
        // use the dialog / parent font
    }
    else
    {
        NONCLIENTMETRICS ncm = {};
        ncm.cbSize = sizeof(ncm);
        ::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        lf = ncm.lfMessageFont;
    }
    m_font.DeleteObject();
    m_fontBold.DeleteObject();
    m_font.CreateFontIndirect(&lf);
    lf.lfWeight = FW_BOLD;
    m_fontBold.CreateFontIndirect(&lf);

    CClientDC dc(pParent ? pParent : CWnd::GetDesktopWindow());
    CFont* pOld = dc.SelectObject(&m_font);
    TEXTMETRIC tm = {};
    dc.GetTextMetrics(&tm);
    m_lineH = tm.tmHeight;
    m_pad   = max(4, m_lineH / 2);
    m_gap   = max(4, m_lineH / 3);
    m_btnH  = m_lineH + max(8, m_lineH / 2);

    int widest = 0;
    for (auto& l : m_labels)
        widest = max(widest, (int)dc.GetTextExtent(l).cx);
    // Wide enough for "Stop" / "Run" swaps and similar short labels
    widest = max(widest, (int)dc.GetTextExtent(_T("Status")).cx);
    m_btnW = widest + 2 * m_lineH;
    dc.SelectObject(pOld);

    m_rowH = max(2 * m_lineH + 2 * m_pad + 4, m_btnH + 2 * m_pad);
}

void CActionList::SetButtonLabels(const std::vector<CString>& labels)
{
    m_labels = labels;
    if (GetSafeHwnd())
    {
        InitFontsAndMetrics(GetParent());
        Layout();
    }
}

void CActionList::SetEmptyText(const CString& text)
{
    m_emptyText = text;
    if (GetSafeHwnd()) Invalidate();
}

// ── Rows ──────────────────────────────────────────────────────────────────────

void CActionList::CreateRowControls(Row& r)
{
    for (auto& label : m_labels)
    {
        auto b = std::make_unique<CButton>();
        b->Create(label, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                  CRect(0, 0, m_btnW, m_btnH), this, m_nextId++);
        b->SetFont(&m_font);
        r.buttons.push_back(std::move(b));
    }
}

int CActionList::AddRow(const CString& title, const CString& subtitle)
{
    auto r = std::make_unique<Row>();
    r->title    = title;
    r->subtitle = subtitle;
    if (GetSafeHwnd()) CreateRowControls(*r);
    m_rows.push_back(std::move(r));
    Layout();
    return (int)m_rows.size() - 1;
}

void CActionList::DeleteRow(int row)
{
    if (row < 0 || row >= GetCount()) return;

    // Keep keyboard focus inside the list if it was on a button being destroyed
    CWnd* pFocus = GetFocus();
    bool focusInRow = false;
    for (auto& b : m_rows[row]->buttons)
        if (pFocus && pFocus->GetSafeHwnd() == b->GetSafeHwnd()) focusInRow = true;

    m_rows.erase(m_rows.begin() + row);      // rows below move up

    int oldSel = m_sel;
    if (m_sel == row)       m_sel = min(row, GetCount() - 1);
    else if (m_sel > row)   --m_sel;

    ScrollTo(m_scroll);                      // clamp and re-layout
    if (focusInRow) SetFocus();
    if (m_sel != oldSel || oldSel == row)
        GetParent()->SendMessage(WM_ACTIONLIST_SELCHANGE, (WPARAM)m_sel, (LPARAM)GetSafeHwnd());
}

void CActionList::DeleteAllRows()
{
    m_rows.clear();
    m_sel = -1;
    m_scroll = 0;
    Layout();
}

void CActionList::SetRowText(int row, const CString& title, const CString& subtitle)
{
    if (row < 0 || row >= GetCount()) return;
    m_rows[row]->title    = title;
    m_rows[row]->subtitle = subtitle;
    InvalidateRect(RowRect(row), FALSE);
}

void CActionList::SetRowProgress(int row, bool bShow, int percent, const CString& caption)
{
    if (row < 0 || row >= GetCount()) return;
    Row& r = *m_rows[row];
    bool bVisibilityChanged = (r.bProgress != bShow);
    r.bProgress = bShow;
    r.percent   = max(0, min(100, percent));
    r.caption   = caption;

    if (bShow && !r.progress && GetSafeHwnd())
    {
        r.progress = std::make_unique<CProgressCtrl>();
        r.progress->Create(WS_CHILD | PBS_SMOOTH, CRect(0, 0, 10, 10), this, m_nextId++);
        r.progress->SetRange(0, 100);
        bVisibilityChanged = true;
    }
    if (r.progress)
    {
        if (r.progress->GetPos() != r.percent) r.progress->SetPos(r.percent);
        if (bVisibilityChanged) Layout();     // shows / hides and positions the bar
    }
    InvalidateRect(RowRect(row), FALSE);
}

void CActionList::SetButtonText(int row, int button, const CString& text)
{
    if (row < 0 || row >= GetCount()) return;
    auto& btns = m_rows[row]->buttons;
    if (button < 0 || button >= (int)btns.size()) return;
    CString cur;
    btns[button]->GetWindowText(cur);
    if (cur != text) btns[button]->SetWindowText(text);
}

void CActionList::EnableButton(int row, int button, bool bEnable)
{
    if (row < 0 || row >= GetCount()) return;
    auto& btns = m_rows[row]->buttons;
    if (button < 0 || button >= (int)btns.size()) return;
    if ((btns[button]->IsWindowEnabled() != FALSE) != bEnable)
        btns[button]->EnableWindow(bEnable ? TRUE : FALSE);
}

// ── Selection ─────────────────────────────────────────────────────────────────

void CActionList::SetSel(int row, bool bNotify)
{
    if (row < -1 || row >= GetCount()) row = -1;
    if (row != m_sel)
    {
        if (m_sel >= 0) InvalidateRect(RowRect(m_sel), FALSE);
        m_sel = row;
        if (m_sel >= 0) InvalidateRect(RowRect(m_sel), FALSE);
        if (bNotify)
            GetParent()->SendMessage(WM_ACTIONLIST_SELCHANGE, (WPARAM)m_sel, (LPARAM)GetSafeHwnd());
    }
    if (row >= 0) EnsureVisible(row);
}

void CActionList::EnsureVisible(int row)
{
    if (row < 0 || row >= GetCount() || !GetSafeHwnd()) return;
    CRect rc;
    GetClientRect(&rc);
    int top = row * m_rowH;
    int bottom = top + m_rowH;
    if (top < m_scroll)                    ScrollTo(top);
    else if (bottom > m_scroll + rc.Height()) ScrollTo(bottom - rc.Height());
}

// ── Geometry / layout ─────────────────────────────────────────────────────────

CRect CActionList::RowRect(int row) const
{
    CRect rc;
    if (GetSafeHwnd()) GetClientRect(&rc);
    int top = row * m_rowH - m_scroll;
    return CRect(0, top, rc.right, top + m_rowH);
}

int CActionList::ButtonsLeft() const
{
    CRect rc;
    GetClientRect(&rc);
    int n = (int)m_labels.size();
    int width = n > 0 ? n * m_btnW + (n - 1) * m_gap : 0;
    return max(m_pad + 4 * m_lineH, (int)rc.right - m_pad - width);
}

int CActionList::MaxScroll() const
{
    CRect rc;
    GetClientRect(&rc);
    return max(0, GetCount() * m_rowH - rc.Height());
}

int CActionList::HitRow(CPoint pt) const
{
    if (pt.y < 0) return -1;
    int row = (pt.y + m_scroll) / m_rowH;
    return row < GetCount() ? row : -1;
}

void CActionList::Layout()
{
    if (!GetSafeHwnd()) return;
    UpdateScrollBar();

    int left = ButtonsLeft();
    int count = 0;
    for (auto& r : m_rows) count += (int)r->buttons.size() + (r->progress ? 1 : 0);
    if (count == 0) { Invalidate(); return; }

    HDWP hdwp = ::BeginDeferWindowPos(count);
    for (int i = 0; i < GetCount(); ++i)
    {
        Row& r = *m_rows[i];
        CRect rr = RowRect(i);
        int y = rr.top + (m_rowH - m_btnH) / 2;
        int x = left;
        for (auto& b : r.buttons)
        {
            if (hdwp)
                hdwp = ::DeferWindowPos(hdwp, b->GetSafeHwnd(), nullptr, x, y, m_btnW, m_btnH,
                                        SWP_NOZORDER | SWP_NOACTIVATE);
            x += m_btnW + m_gap;
        }
        if (r.progress)
        {
            int textLeft  = m_pad;
            int textRight = left - m_pad;
            int barW = max(40, (textRight - textLeft) * 3 / 5);
            int barH = max(8, m_lineH - 4);
            int barY = rr.top + m_pad + m_lineH + 4 + (m_lineH - barH) / 2;
            if (hdwp)
                hdwp = ::DeferWindowPos(hdwp, r.progress->GetSafeHwnd(), nullptr,
                                        textLeft, barY, barW, barH,
                                        SWP_NOZORDER | SWP_NOACTIVATE |
                                        (r.bProgress ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
        }
    }
    if (hdwp) ::EndDeferWindowPos(hdwp);
    Invalidate(FALSE);
}

void CActionList::UpdateScrollBar()
{
    if (!GetSafeHwnd()) return;
    CRect rc;
    GetClientRect(&rc);
    m_scroll = max(0, min(m_scroll, max(0, GetCount() * m_rowH - rc.Height())));

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    si.nMin   = 0;
    si.nMax   = max(0, GetCount() * m_rowH - 1);
    si.nPage  = (UINT)max(0, rc.Height());
    si.nPos   = m_scroll;
    SetScrollInfo(SB_VERT, &si, TRUE);
}

void CActionList::ScrollTo(int pos)
{
    m_scroll = max(0, min(pos, MaxScroll()));
    Layout();
}

// ── Painting ──────────────────────────────────────────────────────────────────

static COLORREF Blend(COLORREF a, COLORREF b, int pctB)
{
    int r = (GetRValue(a) * (100 - pctB) + GetRValue(b) * pctB) / 100;
    int g = (GetGValue(a) * (100 - pctB) + GetGValue(b) * pctB) / 100;
    int bl = (GetBValue(a) * (100 - pctB) + GetBValue(b) * pctB) / 100;
    return RGB(r, g, bl);
}

BOOL CActionList::OnEraseBkgnd(CDC*)
{
    return TRUE;   // everything is painted in OnPaint (double-buffered)
}

void CActionList::OnPaint()
{
    CPaintDC paintDC(this);
    CRect rc;
    GetClientRect(&rc);
    if (rc.IsRectEmpty()) return;

    CDC dc;
    dc.CreateCompatibleDC(&paintDC);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&paintDC, rc.Width(), rc.Height());
    CBitmap* pOldBmp = dc.SelectObject(&bmp);

    const COLORREF clrWindow = ::GetSysColor(COLOR_WINDOW);
    const COLORREF clrText   = ::GetSysColor(COLOR_WINDOWTEXT);
    const COLORREF clrGray   = ::GetSysColor(COLOR_GRAYTEXT);
    const COLORREF clrSel    = Blend(clrWindow, ::GetSysColor(COLOR_HIGHLIGHT), 22);
    const COLORREF clrLine   = Blend(clrWindow, ::GetSysColor(COLOR_3DSHADOW), 45);

    dc.FillSolidRect(rc, clrWindow);
    dc.SetBkMode(TRANSPARENT);
    CFont* pOldFont = dc.SelectObject(&m_font);

    if (m_rows.empty())
    {
        CRect trc = rc;
        trc.DeflateRect(m_pad * 2, m_pad * 3);
        dc.SetTextColor(clrGray);
        dc.DrawText(m_emptyText, trc, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
    }

    int left = ButtonsLeft();
    for (int i = 0; i < GetCount(); ++i)
    {
        CRect rr = RowRect(i);
        if (rr.bottom < 0 || rr.top > rc.bottom) continue;
        Row& r = *m_rows[i];

        if (i == m_sel) dc.FillSolidRect(rr, clrSel);

        // Title
        CRect trc(m_pad, rr.top + m_pad, left - m_pad, rr.top + m_pad + m_lineH);
        dc.SelectObject(&m_fontBold);
        dc.SetTextColor(clrText);
        dc.DrawText(r.title, trc, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        // Second line: subtitle or progress caption
        dc.SelectObject(&m_font);
        CRect src(m_pad, trc.bottom + 4, left - m_pad, trc.bottom + 4 + m_lineH);
        if (r.bProgress)
        {
            int barW = max(40, (src.Width()) * 3 / 5);
            CRect crc = src;
            crc.left += barW + m_gap;
            dc.SetTextColor(clrText);
            dc.DrawText(r.caption, crc, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS |
                                        DT_NOPREFIX | DT_VCENTER);
        }
        else
        {
            dc.SetTextColor(clrGray);
            dc.DrawText(r.subtitle, src, DT_LEFT | DT_SINGLELINE | DT_PATH_ELLIPSIS |
                                         DT_NOPREFIX);
        }

        // Separator
        dc.FillSolidRect(0, rr.bottom - 1, rc.Width(), 1, clrLine);

        if (i == m_sel && GetFocus() == this)
        {
            CRect frc = rr;
            frc.DeflateRect(1, 1, 1, 2);
            dc.DrawFocusRect(frc);
        }
    }

    dc.SelectObject(pOldFont);
    paintDC.BitBlt(0, 0, rc.Width(), rc.Height(), &dc, 0, 0, SRCCOPY);
    dc.SelectObject(pOldBmp);
}

// ── Messages ──────────────────────────────────────────────────────────────────

void CActionList::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    ScrollTo(m_scroll);
}

void CActionList::OnVScroll(UINT nSBCode, UINT /*nPos*/, CScrollBar* /*pScrollBar*/)
{
    CRect rc;
    GetClientRect(&rc);
    int pos = m_scroll;
    switch (nSBCode)
    {
    case SB_LINEUP:   pos -= m_rowH / 2;      break;
    case SB_LINEDOWN: pos += m_rowH / 2;      break;
    case SB_PAGEUP:   pos -= rc.Height();     break;
    case SB_PAGEDOWN: pos += rc.Height();     break;
    case SB_TOP:      pos = 0;                break;
    case SB_BOTTOM:   pos = MaxScroll();      break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION:
    {
        SCROLLINFO si = {};
        si.cbSize = sizeof(si);
        si.fMask  = SIF_TRACKPOS;
        GetScrollInfo(SB_VERT, &si);
        pos = si.nTrackPos;
        break;
    }
    default: return;
    }
    ScrollTo(pos);
}

BOOL CActionList::OnMouseWheel(UINT /*nFlags*/, short zDelta, CPoint /*pt*/)
{
    ScrollTo(m_scroll - zDelta * m_rowH / WHEEL_DELTA);
    return TRUE;
}

void CActionList::OnLButtonDown(UINT nFlags, CPoint point)
{
    SetFocus();
    int row = HitRow(point);
    if (row >= 0) SetSel(row, true);
    CWnd::OnLButtonDown(nFlags, point);
}

void CActionList::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    int row = HitRow(point);
    if (row >= 0)
    {
        SetSel(row, true);
        GetParent()->SendMessage(WM_ACTIONLIST_DBLCLK, (WPARAM)row, (LPARAM)GetSafeHwnd());
    }
    CWnd::OnLButtonDblClk(nFlags, point);
}

UINT CActionList::OnGetDlgCode()
{
    return DLGC_WANTARROWS;
}

void CActionList::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    int n = GetCount();
    if (n > 0)
    {
        int sel = m_sel;
        switch (nChar)
        {
        case VK_UP:    sel = sel <= 0 ? 0 : sel - 1;          break;
        case VK_DOWN:  sel = sel < 0 ? 0 : min(n - 1, sel + 1); break;
        case VK_HOME:  sel = 0;                               break;
        case VK_END:   sel = n - 1;                           break;
        case VK_PRIOR: sel = max(0, sel - 5);                 break;
        case VK_NEXT:  sel = min(n - 1, max(0, sel) + 5);     break;
        default:
            CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
            return;
        }
        SetSel(sel, true);
        return;
    }
    CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CActionList::OnSetFocus(CWnd* pOldWnd)
{
    CWnd::OnSetFocus(pOldWnd);
    if (m_sel >= 0) InvalidateRect(RowRect(m_sel), FALSE);
}

void CActionList::OnKillFocus(CWnd* pNewWnd)
{
    CWnd::OnKillFocus(pNewWnd);
    if (m_sel >= 0) InvalidateRect(RowRect(m_sel), FALSE);
}

void CActionList::OnSysColorChange()
{
    CWnd::OnSysColorChange();
    Invalidate();
}

BOOL CActionList::OnCommand(WPARAM wParam, LPARAM lParam)
{
    if (HIWORD(wParam) == BN_CLICKED)
    {
        HWND hCtl = (HWND)lParam;
        for (int i = 0; i < GetCount(); ++i)
        {
            auto& btns = m_rows[i]->buttons;
            for (int b = 0; b < (int)btns.size(); ++b)
            {
                if (btns[b]->GetSafeHwnd() == hCtl)
                {
                    SetSel(i, true);
                    // Posted, not sent: the handler may delete this very row
                    // (and its buttons) — never do that inside the button's
                    // own click notification.
                    GetParent()->PostMessage(WM_ACTIONLIST_BUTTON,
                                             MAKEWPARAM(i, b), (LPARAM)GetSafeHwnd());
                    return TRUE;
                }
            }
        }
    }
    return CWnd::OnCommand(wParam, lParam);
}
