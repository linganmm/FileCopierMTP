# FileCopierMTP — Android ↔ PC File Copier (MFC + WPD/MTP)

A Windows MFC application that copies files between a Windows PC and an
Android device connected via USB in **MTP (Media Transfer Protocol)** mode,
preserving the complete sub-folder hierarchy in both directions.

---

## LXFreeBack — backup plans (startup window)

The program now starts with an SDI window named **LXFreeBack** that manages
*backup plans*. A plan is a named list of items; each item is one
"source files/folders → destination folder" pair, chosen with the same
device/folder browser as the Universal File Copier (local drives and MTP
devices in any direction).

| Where | What it does |
|---|---|
| **File → New Backup Plan…** (Ctrl+N) | Opens *New Backup Plan*: optional title, **Add Item…**, and per-item **Edit** / **Delete** buttons |
| **File → Load Backup Plan…** (Ctrl+O) | Opens one or more `.lbp` files and lists them |
| **File → Universal File Copier…** | The original one-off copy dialog, unchanged |
| **Help → About LXFreeBack…** | Program name, version, description and copyright (Luoxuan Intelligent Technology Co., Ltd) |
| Row buttons / **Tool** menu | **Edit**, **Unload**, **Remove**, **Run** (becomes **Stop** while running), **Status** — the Tool menu acts on the selected plan |

**Saving** (*New/Edit Backup Plan* dialog)

* **Save** writes `<title>.lbp` to the plan's current folder: the folder of
  the executable for a new plan, the plan file's own folder for a loaded one.
* **Save As…** asks for a folder, saves there, and makes it the current folder.
* **Save & Close** saves to the current folder and closes; **Cancel** closes
  without saving.
* A blank title becomes `LXBackupplanN`, with the smallest N that gives a file
  name not already used in the executable's folder.
* Changing the title of a saved plan renames its file.
* A plan appears in the main list once it has been saved.

**Exclusions** (*Add/Edit Item* dialog). The optional **Exclusion:** row picks
files and folders *inside* the source that must not be copied, using the same
Ctrl+Click browser as **Source** (it opens on the source's device with the
source tree already expanded). An excluded folder is skipped together with
everything under it, and is not even scanned. Example: source `A\` with
sub-folders `a1\`, `a2\`, `a3\` and exclusion `A\a3` copies `A\a1` and
`A\a2` only. Exclusions must be on the same device as the source; anything
outside the selected source folders is removed with a notice. **Clear**
removes all exclusions. Older `.lbp` files without exclusions still load.

**If file exists** (*Add/Edit Item* dialog). Each item has a dropdown —
**Overwrite**, **Skip** or **Overwrite if newer** (the default) — matching the
buttons of the *File Conflict* dialog. When a plan runs, existing files at the
destination are handled with the item's choice without asking. Plans saved
before this option existed use **Overwrite if newer**. The Universal File
Copier still asks each time.

**Running.** A row shows a progress bar while its plan runs; several plans
can run at the same time. **Status** shows the progress and the log, using
the same messages as the Universal File Copier log box. Missing local
destination folders are created. Before copying, MTP items are looked up
again by device name and folder path, because Android renumbers object IDs
on every connection.

**Remove** asks for confirmation, then deletes the `.lbp` file and removes the
plan from the list. **Unload** only removes it from the list.

### `.lbp` file format (UTF-8 JSON)

A plan stores only the **device name** and the **path below the device root**
(`subPath`), with no device IDs, MTP object IDs or absolute paths. When a plan
runs, each device or drive is found by name: a phone by its friendly name; a
drive by its "Label (D:)" name, or, if the letter changed, by its volume label
when exactly one drive has that label. The real path or object ID is then
rebuilt from `subPath`. Older plan files that still contain IDs and absolute
paths load and run; they are written in the new form the next time they are
saved.

```json
{
  "format": "LXFreeBack Backup Plan",
  "version": 1,
  "name": "LXBackupplan1",
  "created": "2026-10-26T17:05:03+08:00",
  "modified": "2026-10-26T17:20:41+08:00",
  "items": [
    {
      "source": {
        "type": "mtp",
        "deviceName": "Pixel 8",
        "entries": [
          { "name": "Camera", "isFolder": true,
            "subPath": "Internal shared storage/DCIM/Camera" }
        ],
        "exclusions": [
          { "name": "Thumbnails", "isFolder": true,
            "subPath": "Internal shared storage/DCIM/Camera/Thumbnails" }
        ]
      },
      "onConflict": "overwriteIfNewer",
      "destination": {
        "type": "local",
        "deviceName": "Data (D:)",
        "subPath": "Backup\\Phone"
      }
    }
  ]
}
```

MTP sub paths use `/`; local sub paths use `\` and exclude the drive root.

---

## Feature Summary

| Feature | Detail |
|---|---|
| Direction | **PC → Android** or **Android → PC** (combo-box switch) |
| MTP browser | Tree-view dialog that lists all connected MTP/Android devices and lets you navigate their folder hierarchy |
| Hierarchy preservation | Sub-folder structure is replicated exactly at the destination |
| Conflict detection | Detects files that already exist at the same relative path |
| Conflict dialog | **Overwrite / Skip / Cancel** with an **"Apply to all"** checkbox |
| Progress bar | Live file count and percentage |
| Status labels | Shows the relative path of the file being copied |
| Activity log | Scrollable list; errors and a final summary are appended |
| Cancel | Gracefully interrupts the worker thread |
| Background thread | UI stays fully responsive during long transfers |

---

## File Layout

```
FileCopierMTP/
├── pch.h / pch.cpp            Precompiled header
├── resource.h                  All control / command IDs & WM_ messages
├── FileCopierMTP.h/.cpp        CWinApp entry point (starts the LXFreeBack SDI window)
├── MainFrm.h/.cpp              LXFreeBack frame window (menu, status bar)
├── LXFreeBackDoc.h/.cpp        Document: the list of loaded plans
├── LXFreeBackView.h/.cpp       View: plan rows, Tool menu, runs plans
├── PlanDlg.h/.cpp              "New / Edit Backup Plan" dialog
├── ItemDlg.h/.cpp              "Add / Edit Item" dialog
├── PlanStatusDlg.h/.cpp        "Status" (progress + log) dialog
├── ActionList.h/.cpp           Scrolling row list with per-row buttons + progress bar
├── PlanEntry.h                 A loaded plan + its run state and log
├── BackupPlan.h/.cpp           Plan model, .lbp save/load, default titles
├── JsonLite.h/.cpp             Small JSON reader/writer (no MFC, no PCH)
├── CopyEngine.h/.cpp           CCopyJob: worker-thread copy engine (per job state)
├── MainDlg.h/.cpp              Universal File Copier dialog (uses CCopyJob)
├── MtpBrowserDlg.h/.cpp        MTP device & folder tree browser
├── ConflictDialog.h/.cpp       File-conflict prompt
├── WpdHelper.h/.cpp            WPD/MTP COM abstraction layer
├── res/LXFreeBack.ico          Application icon
├── FileCopierMTP.rc            Dialogs, menu, accelerators, strings
└── FileCopierMTP.vcxproj       Visual Studio 2022 project
```

---

## Architecture

### Thread model

```
UI Thread                              Worker Thread (CCopyJob::ThreadProc)
─────────────────────────────          ──────────────────────────────────────
DoModal()
  CCopyJob::Start ──AfxBeginThread──►   CoInitializeEx(COINIT_MULTITHREADED)
                                       CWpdHelper::OpenDevice()
  OnCopyProgress() ◄──PostMessage──   for each file:
    update progress bar                  PostMessage(WM_COPY_PROGRESS)
                                         if conflict detected:
  OnCopyConflict() ◄──PostMessage──         PostMessage(WM_COPY_CONFLICT)
    show CConflictDialog                    spin-wait on bConflictReady
    write result + applyAll flag       read resolution from CCopyState
    set bConflictReady = TRUE          perform copy (WPD or fs::copy_file)
                                       PostMessage(WM_COPY_DONE)
  OnCopyDone() ◄──PostMessage──
    re-enable controls, add summary
```

### WpdHelper layer

| Method | Purpose |
|---|---|
| `EnumerateDevices()` | Static. Calls `IPortableDeviceManager::GetDevices()`. |
| `OpenDevice()` | Opens `IPortableDevice` + `IPortableDeviceContent` + `IPortableDeviceProperties`. |
| `GetChildren()` | Calls `IPortableDeviceContent::EnumObjects()` for one folder. |
| `FindChild()` | Linear scan of `GetChildren()` by name (case-insensitive). |
| `CreateFolder()` | `IPortableDeviceContent::CreateObjectWithPropertiesOnly()`. |
| `CopyFromDevice()` | `IPortableDeviceResources::GetStream()` → reads stream → writes local file. |
| `CopyToDevice()` | `IPortableDeviceContent::CreateObjectWithPropertiesAndData()` → streams local file to device. |
| `DeleteObject()` | `IPortableDeviceContent::Delete()` (used before overwrite). |
| `EnumerateFiles()` | Recursive DFS over the device tree, fires callback per file. |

### MTP browser dialog (`CMtpBrowserDlg`)

* Loads all MTP devices into a combo-box on `OnInitDialog`.
* Populates the `SysTreeView32` lazily: each folder node starts with a dummy
  child; on `TVN_ITEMEXPANDING` the dummy is replaced with real children fetched
  from the device.
* Stores the WPD object-ID (e.g. `"o2FF"`) as the `lParam` of each tree item.
* On OK, exposes `m_strSelectedDeviceId` and `m_strSelectedObjectId` to the
  calling dialog.

---

## Building

### Requirements

* **Visual Studio 2019 or 2022** — any edition (Community is fine)
* Workload: **Desktop development with C++**
  * Component: **MFC for latest build tools**
* Windows SDK 10.0 (installed with the workload)

The WPD headers and `PortableDeviceGuids.lib` ship with every Windows SDK;
no extra downloads are needed.

### Steps

1. Open `FileCopierMTP.vcxproj` in Visual Studio.
2. Choose `Debug | x64` or `Release | x64`.
3. **Build → Build Solution** (`Ctrl+Shift+B`).
4. Run `x64\Debug\FileCopierMTP.exe`.

### MSBuild (command-line)

```cmd
msbuild FileCopierMTP.vcxproj /p:Configuration=Release /p:Platform=x64
```

---

## Copyright
Copyright 2026 Beijing Luoxuan Intelligent Technology Co. Ltd.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.