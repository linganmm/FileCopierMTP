#pragma once
#include "pch.h"
#include "resource.h"

// ─────────────────────────────────────────────────────────────────────────────
//  CActionList — a scrolling list whose rows show a bold title, a grey second
//  line (or a progress bar) and a row of push buttons on the right-hand side.
//
//  Used for the items of a backup plan (Edit / Delete) and for the plans in
//  the LXFreeBack main window (Edit / Unload / Remove / Run / Status).
//
//  Notifications to the parent window:
//    WM_ACTIONLIST_BUTTON     wParam = MAKEWPARAM(row, button)   (posted)
//    WM_ACTIONLIST_SELCHANGE  wParam = row or -1                 (sent)
//    WM_ACTIONLIST_DBLCLK     wParam = row                       (sent)
//    lParam is always the list's HWND.
// ─────────────────────────────────────────────────────────────────────────────
class CActionList : public CWnd
{
public:
    CActionList();
    virtual ~CActionList();

    BOOL Create(const CRect& rc, CWnd* pParent, UINT nID);

    // Labels of the buttons shown on every row; call before adding rows.
    void SetButtonLabels(const std::vector<CString>& labels);
    // Text shown when the list has no rows.
    void SetEmptyText(const CString& text);

    int  GetCount() const { return (int)m_rows.size(); }
    int  AddRow(const CString& title, const CString& subtitle);
    void DeleteRow(int row);
    void DeleteAllRows();

    void SetRowText(int row, const CString& title, const CString& subtitle);
    // Shows (or hides) a progress bar with a short caption in place of the
    // subtitle line.
    void SetRowProgress(int row, bool bShow, int percent, const CString& caption);
    void SetButtonText(int row, int button, const CString& text);
    void EnableButton(int row, int button, bool bEnable);

    int  GetSel() const { return m_sel; }
    void SetSel(int row, bool bNotify = false);
    void EnsureVisible(int row);

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg UINT OnGetDlgCode();
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnKillFocus(CWnd* pNewWnd);
    afx_msg void OnSysColorChange();
    virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
    DECLARE_MESSAGE_MAP()

private:
    struct Row
    {
        CString title;
        CString subtitle;
        bool    bProgress = false;
        int     percent   = 0;
        CString caption;
        std::vector<std::unique_ptr<CButton>> buttons;
        std::unique_ptr<CProgressCtrl>        progress;
        ~Row();
    };

    std::vector<std::unique_ptr<Row>> m_rows;
    std::vector<CString> m_labels;
    CString m_emptyText;

    CFont m_font;
    CFont m_fontBold;

    int  m_lineH   = 16;
    int  m_pad     = 8;
    int  m_gap     = 6;
    int  m_rowH    = 48;
    int  m_btnW    = 64;
    int  m_btnH    = 24;
    int  m_scroll  = 0;
    int  m_sel     = -1;
    UINT m_nextId  = 100;

    void    InitFontsAndMetrics(CWnd* pParent);
    void    CreateRowControls(Row& r);
    void    Layout();
    void    UpdateScrollBar();
    void    ScrollTo(int pos);
    CRect   RowRect(int row) const;           // client coordinates
    int     ButtonsLeft() const;              // x where the button block starts
    int     HitRow(CPoint pt) const;
    int     MaxScroll() const;
};
