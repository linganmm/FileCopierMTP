#pragma once
#include "pch.h"
#include "resource.h"
#include "WpdHelper.h"
#include "BrowserState.h"

#include <afxwin.h>
#include <afxdialogex.h>

class CMtpBrowserDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CMtpBrowserDlg)
public:
    explicit CMtpBrowserDlg(CWnd* pParent = nullptr);
    virtual ~CMtpBrowserDlg();

    enum { IDD = IDD_BROWSE_MTP_DIALOG };

    // Set before DoModal()
    bool           m_bSourceMode = false;
    CBrowserState* m_pState      = nullptr;

    // Results
    bool    m_bIsLocalDisk = false;
    CString m_strSelectedDeviceId;
    CString m_strSelectedDisplayPath;

    // Destination mode
    CString m_strSelectedObjectId;
    CString m_strSelectedLocalPath;

    // Source mode — filled on OK
    std::vector<CSourceItem> m_selectedItems;

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual void OnOK();
    virtual void OnCancel();

    afx_msg void OnCbnSelchangeDevice();
    afx_msg void OnTvnItemExpandingTree(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnTvnSelchangedTree   (NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnNmClickTree         (NMHDR* pNMHDR, LRESULT* pResult);

    DECLARE_MESSAGE_MAP()

private:
    CComboBox   m_comboDevice;
    CTreeCtrl   m_treeCtrl;
    CStatic     m_staticSelPath;
    CStatic     m_staticSelCount;
    CButton     m_btnOk;
    CButton     m_btnCancel;

    std::unique_ptr<CWpdHelper> m_pWpd;
    std::vector<MtpDeviceInfo>  m_devices;

    // Ctrl+click multi-select set (source mode only)
    std::set<CSourceItem*>      m_multiSel;

    void LoadDevice(int idx);
    void PopulateTreeChildren(HTREEITEM hParent, const CString& strParentId,
                              bool bFilesToo);
    CString BuildDisplayPath(HTREEITEM hItem);
    void    UpdateSelectionCount();
    void    SetItemHighlight(HTREEITEM hItem, bool bHighlight);
    bool    IsItemSelected(HTREEITEM hItem) const;
    void    CollectSelected(HTREEITEM hItem);

    void SaveState();
    void RestoreTree();
    void SaveTreeNode(HTREEITEM hItem, int depth,
                      std::vector<TreeNodeSnapshot>& out);

    void AdjustSourceDialog();
    void AdjustDestDialog();

    std::vector<CSourceItem*> m_itemData;
    void ClearItemData();

    static const TCHAR* kDummyId;
};
