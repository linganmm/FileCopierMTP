#pragma once
#include "pch.h"
#include "resource.h"
#include "LXFreeBackDoc.h"
#include "ActionList.h"

// ─────────────────────────────────────────────────────────────────────────────
//  The LXFreeBack main view: the list of loaded backup plans, each with
//  Edit / Unload / Remove / Run / Status buttons.  The same five actions are
//  on the Tool menu and act on the selected plan.
// ─────────────────────────────────────────────────────────────────────────────
class CLXFreeBackView : public CView
{
protected:
    CLXFreeBackView();
    DECLARE_DYNCREATE(CLXFreeBackView)

public:
    virtual ~CLXFreeBackView();
    CLXFreeBackDoc* GetDocument() const;

    virtual void OnDraw(CDC* pDC);
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

    // Loads a .lbp file and adds it to the list (or selects it if it is
    // already loaded).
    bool LoadPlanFile(const CString& path, bool bReportErrors);

    // Called by the frame before closing; stops running plans after asking.
    bool PrepareToClose();

protected:
    afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);

    afx_msg void OnFileNewPlan();
    afx_msg void OnFileLoadPlan();
    afx_msg void OnFileQuickCopy();

    afx_msg void OnToolEdit();
    afx_msg void OnToolUnload();
    afx_msg void OnToolRemove();
    afx_msg void OnToolRun();
    afx_msg void OnToolStatus();
    afx_msg void OnUpdateToolEdit(CCmdUI* pCmdUI);
    afx_msg void OnUpdateToolUnload(CCmdUI* pCmdUI);
    afx_msg void OnUpdateToolRemove(CCmdUI* pCmdUI);
    afx_msg void OnUpdateToolRun(CCmdUI* pCmdUI);
    afx_msg void OnUpdateToolStatus(CCmdUI* pCmdUI);

    afx_msg LRESULT OnListButton(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnListDblClk(WPARAM wParam, LPARAM lParam);

    afx_msg LRESULT OnCopyProgress(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnCopyConflict(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnCopyDone(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnCopyError(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnCopyLog(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

private:
    enum { BTN_EDIT = 0, BTN_UNLOAD, BTN_REMOVE, BTN_RUN, BTN_STATUS };

    CActionList m_list;
    bool        m_bClosing = false;

    std::vector<std::shared_ptr<CPlanEntry>>& Plans() const;
    int  SelectedPlan() const;       // -1 if none
    bool IsRunning(int index) const;

    void AddEntry(const CBackupPlan& plan);
    void RemoveDuplicatesOf(const std::shared_ptr<CPlanEntry>& keep);
    int  IndexOf(const std::shared_ptr<CPlanEntry>& entry) const;
    void RefreshRow(int index);
    void SetStatusText(const CString& text);

    void EditPlan(int index);
    void UnloadPlan(int index);
    void RemovePlan(int index);
    void RunPlan(int index);
    void ShowStatus(int index);

    static CString DateTimeNow();    // "2026-10-26 17:05:03"
};
