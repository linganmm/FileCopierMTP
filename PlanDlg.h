#pragma once
#include "pch.h"
#include "resource.h"
#include "BackupPlan.h"
#include "ActionList.h"

#include <afxdialogex.h>

// ─────────────────────────────────────────────────────────────────────────────
//  "New Backup Plan" / "Edit Backup Plan"
//
//  Save        → writes "<title>.lbp" to the plan's current folder (the
//                executable's folder for a new plan, the file's folder for a
//                loaded plan, or the folder last chosen with Save As).
//  Save As     → asks for a folder, saves there; that folder becomes current.
//  Save&Close  → Save, then close.
//  Cancel      → closes without saving.
//
//  After DoModal, m_bSaved tells whether the plan was written at least once;
//  m_savedPlan is then the version on disk (title, file path, times).
// ─────────────────────────────────────────────────────────────────────────────
class CPlanDlg : public CDialogEx
{
public:
    CPlanDlg(const CBackupPlan& plan, bool bEditMode, CWnd* pParent = nullptr);
    enum { IDD = IDD_PLAN_DIALOG };

    bool        m_bSaved = false;
    CBackupPlan m_savedPlan;

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual void OnOK();        // Enter key — handled by the default button
    virtual void OnCancel();

    afx_msg void OnBtnAddItem();
    afx_msg void OnBtnSave();
    afx_msg void OnBtnSaveAs();
    afx_msg void OnBtnSaveClose();
    afx_msg LRESULT OnListButton(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnListDblClk(WPARAM wParam, LPARAM lParam);
    DECLARE_MESSAGE_MAP()

private:
    CBackupPlan  m_plan;
    bool         m_bEditMode;
    CString      m_strDir;            // folder that Save writes to

    CEdit        m_editTitle;
    CStatic      m_staticHint;
    CStatic      m_staticFile;
    CStatic      m_staticTimes;
    CActionList  m_list;

    // Shared by all Add/Edit Item dialogs opened from this plan
    CBrowserState m_srcState;
    CBrowserState m_exclState;
    CBrowserState m_dstState;

    enum { BTN_EDIT = 0, BTN_DELETE = 1 };

    bool SaveTo(const CString& dir);
    void EditItem(int index);
    void DeleteItem(int index);
    void RefreshRows(int fromIndex = 0);
    void RefreshInfo();
    CString RowTitle(int index) const;
    CString RowSubtitle(int index) const;
};
