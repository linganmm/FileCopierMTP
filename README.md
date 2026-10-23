# FileCopierMTP — Android ↔ PC File Copier (MFC + WPD/MTP)

A Windows MFC application that copies files between a Windows PC and an
Android device connected via USB in **MTP (Media Transfer Protocol)** mode,
preserving the complete sub-folder hierarchy in both directions.

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
├── resource.h                  All control IDs & WM_ messages
├── FileCopierMTP.h/.cpp        CWinApp entry point
├── MainDlg.h/.cpp              Main dialog + copy worker thread
├── MtpBrowserDlg.h/.cpp        MTP device & folder tree browser
├── ConflictDialog.h/.cpp       File-conflict prompt
├── WpdHelper.h/.cpp            WPD/MTP COM abstraction layer
├── FileCopierMTP.rc            Dialog resource layouts
└── FileCopierMTP.vcxproj       Visual Studio 2022 project
```

---

## Architecture

### Thread model

```
UI Thread                              Worker Thread (ThreadProc)
─────────────────────────────          ──────────────────────────────────────
DoModal()
  OnBtnStart()  ──AfxBeginThread──►   CoInitializeEx(COINIT_MULTITHREADED)
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