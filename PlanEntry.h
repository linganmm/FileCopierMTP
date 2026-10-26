#pragma once
#include "pch.h"
#include "BackupPlan.h"
#include "CopyEngine.h"

// One backup plan shown in the LXFreeBack window, plus its run state.
// Only touched on the UI thread (the copy job reports by PostMessage).
struct CPlanEntry
{
    CBackupPlan plan;

    std::shared_ptr<CCopyJob> job;     // current / last run
    bool    bRunning = false;          // set on Run, cleared when WM_COPY_DONE arrives
    int     nPercent = 0;              // overall progress of the current run
    CString strStatus;                 // e.g. "Item 1/2 - copying 12 of 240"
    CString strCurrentFile;
    CString strLastRun;                // summary of the last finished run

    int     nRunSerial = 0;            // incremented at each Run (the log is cleared then)
    std::vector<CString> log;          // same lines as the Universal File Copier log box
};
