#pragma once

// ── Dialogs ─────────────────────────────────────────────────────────────────
#define IDD_MAIN_DIALOG             102
#define IDD_CONFLICT_DIALOG         103
#define IDD_BROWSE_MTP_DIALOG       104

// ── Main dialog controls ─────────────────────────────────────────────────────
#define IDC_EDIT_SRC_PATH           1001   // read-only, shows selected source
#define IDC_BTN_BROWSE_SRC          1002
#define IDC_EDIT_DST_PATH           1003   // read-only, shows selected destination
#define IDC_BTN_BROWSE_DST          1004
#define IDC_BTN_START               1005
#define IDC_BTN_CANCEL_COPY         1006
#define IDC_PROGRESS                1007
#define IDC_STATIC_STATUS           1008
#define IDC_STATIC_FILE             1009
#define IDC_LIST_LOG                1010
#define IDC_STATIC_SRC_LABEL        1011
#define IDC_STATIC_DST_LABEL        1012
#define IDC_STATIC_SRC_TYPE         1013   // small label e.g. "[Local]" / "[MTP]"
#define IDC_STATIC_DST_TYPE         1014

// ── MTP/device browser dialog controls ──────────────────────────────────────
#define IDC_COMBO_DEVICE            2001
#define IDC_TREE_MTP                2002
#define IDC_BTN_OK              2003
#define IDC_BTN_CANCEL          2004
#define IDC_STATIC_SEL_PATH         2005

// ── Conflict dialog controls ─────────────────────────────────────────────────
#define IDC_STATIC_CONFLICT_MSG     3001
#define IDC_CHK_APPLY_ALL           3002
#define IDC_BTN_OVERWRITE           3003
#define IDC_BTN_SKIP                3004
#define IDC_BTN_CANCEL_OP           3005
#define IDC_BTN_NEWER               3006

// ── Thread messages ──────────────────────────────────────────────────────────
#define WM_COPY_PROGRESS            (WM_USER + 100)
#define WM_COPY_CONFLICT            (WM_USER + 101)
#define WM_COPY_DONE                (WM_USER + 102)
#define WM_COPY_ERROR               (WM_USER + 103)
#define WM_COPY_LOG                 (WM_USER + 104)   // informational log line

// ── Conflict resolution ──────────────────────────────────────────────────────
#define CONFLICT_OVERWRITE          1
#define CONFLICT_SKIP               2
#define CONFLICT_CANCEL             3
#define CONFLICT_NEWER              4

// ── Additional browser controls (multi-select) ───────────────────────────────
#define IDC_STATIC_SEL_COUNT        2006   // "N items selected"
#define IDC_STATIC_SEL_PATH_LABEL   2009


// ═════════════════════════════════════════════════════════════════════════════
//  LXFreeBack (SDI shell + backup plans)
// ═════════════════════════════════════════════════════════════════════════════

// ── Frame resources (menu, accelerators, icon, doc-template string) ─────────
#define IDR_MAINFRAME               128

// ── Dialogs ─────────────────────────────────────────────────────────────────
#define IDD_PLAN_DIALOG             105   // "New Backup Plan" / "Edit Backup Plan"
#define IDD_ITEM_DIALOG             106   // "Add Item" / "Edit Item"
#define IDD_PLAN_STATUS_DIALOG      107   // "Status" log viewer
#define IDD_ABOUTBOX                108   // Help > About

// ── Plan dialog controls ─────────────────────────────────────────────────────
#define IDC_EDIT_PLAN_TITLE         4001
#define IDC_STATIC_TITLE_HINT       4002
#define IDC_BTN_ADD_ITEM            4003
#define IDC_ITEM_LIST_FRAME         4004  // placeholder where the item list is created
#define IDC_STATIC_PLAN_FILE        4005
#define IDC_STATIC_PLAN_TIMES       4006
#define IDC_BTN_SAVE                4007
#define IDC_BTN_SAVE_AS             4008
#define IDC_BTN_SAVE_CLOSE          4009
#define IDC_ITEM_LIST               4010  // the CActionList control itself
#define IDC_STATIC_ITEMS_LABEL      4011

// ── Item dialog: exclusion row ───────────────────────────────────────────────
#define IDC_STATIC_EXCL_LABEL       4301
#define IDC_STATIC_EXCL_TYPE        4302
#define IDC_EDIT_EXCL_PATH          4303
#define IDC_BTN_BROWSE_EXCL         4304
#define IDC_BTN_CLEAR_EXCL          4305
#define IDC_STATIC_CONFLICT_LABEL   4306
#define IDC_COMBO_CONFLICT          4307   // "If file exists:" strategy

// ── Item dialog controls (re-uses the main dialog's source/dest IDs) ────────
//    IDC_EDIT_SRC_PATH, IDC_BTN_BROWSE_SRC, IDC_EDIT_DST_PATH, IDC_BTN_BROWSE_DST,
//    IDC_STATIC_SRC_TYPE, IDC_STATIC_DST_TYPE, IDOK, IDCANCEL

// ── Status dialog controls ───────────────────────────────────────────────────
#define IDC_STATIC_STATUS_TITLE     4101
#define IDC_STATIC_STATUS_FILE      4102
//    IDC_PROGRESS, IDC_STATIC_STATUS, IDC_STATIC_FILE, IDC_LIST_LOG re-used

// ── About box ────────────────────────────────────────────────────────────────
#define IDC_STATIC_ABOUT_VERSION    4401
#define IDC_STATIC_ABOUT_COPYRIGHT  4402

// ── Main view ────────────────────────────────────────────────────────────────
#define IDC_PLAN_LIST               4201

// ── Commands ─────────────────────────────────────────────────────────────────
#define ID_FILE_NEW_PLAN            32771
#define ID_FILE_LOAD_PLAN           32772
#define ID_FILE_QUICK_COPY          32773
#define ID_TOOL_EDIT_PLAN           32781
#define ID_TOOL_UNLOAD_PLAN         32782
#define ID_TOOL_REMOVE_PLAN         32783
#define ID_TOOL_RUN_PLAN            32784
#define ID_TOOL_STATUS_PLAN         32785

// ── Row-list control notifications (sent to the parent window) ──────────────
//    wParam = MAKEWPARAM(row, buttonIndex), lParam = HWND of the list
#define WM_ACTIONLIST_BUTTON        (WM_USER + 200)
//    wParam = new selected row (-1 = none), lParam = HWND of the list
#define WM_ACTIONLIST_SELCHANGE     (WM_USER + 201)
//    wParam = row, lParam = HWND of the list
#define WM_ACTIONLIST_DBLCLK        (WM_USER + 202)

#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE    129
#define _APS_NEXT_COMMAND_VALUE     32790
#define _APS_NEXT_CONTROL_VALUE     4308
#define _APS_NEXT_SYMED_VALUE       101
#endif
#endif
