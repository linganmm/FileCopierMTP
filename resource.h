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

// ── Conflict resolution ──────────────────────────────────────────────────────
#define CONFLICT_OVERWRITE          1
#define CONFLICT_SKIP               2
#define CONFLICT_CANCEL             3
#define CONFLICT_NEWER              4

// ── Additional browser controls (multi-select) ───────────────────────────────
#define IDC_STATIC_SEL_COUNT        2006   // "N items selected"
#define IDC_STATIC_SEL_PATH_LABEL   2009
