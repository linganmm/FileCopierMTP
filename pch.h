#pragma once

// MFC
#include <afxwin.h>
#include <afxext.h>
#include <afxdlgs.h>
#include <afxcmn.h>
#include <afxmt.h>
#include <afxdialogex.h>

// Windows / COM / Shell
#include <windows.h>
#include <windowsx.h>   // GET_X_LPARAM, GET_Y_LPARAM
#include <shlobj.h>
#include <shlwapi.h>
#include <comdef.h>
#include <atlbase.h>
#include <atlcom.h>

// Windows Portable Devices (MTP/WPD)
// Order matters: PortableDeviceTypes.h must come before PortableDevice.h
#include <portabledeviceapi.h>
#include <portabledevicetypes.h>
#include <portabledevice.h>

// ── WPD GUIDs that are missing or inconsistent across SDK versions ────────────
// Defined in WpdGuids.cpp (one translation unit) and declared extern here.
// This avoids duplicate-symbol linker errors that DEFINE_GUID in a header causes.

extern const GUID WPD_CONTENT_TYPE_FOLDER_GUID;
extern const GUID WPD_CONTENT_TYPE_FUNCTIONAL_OBJECT_GUID;
extern const GUID WPD_CONTENT_TYPE_STORAGE_GUID;
extern const GUID WPD_CONTENT_TYPE_GENERIC_FILE_GUID;
extern const GUID WPD_CONTENT_TYPE_ALL_GUID;

extern const GUID WPD_OBJECT_FORMAT_UNSPECIFIED_GUID;
extern const GUID WPD_OBJECT_FORMAT_PROPERTIES_ONLY_GUID;
extern const GUID WPD_OBJECT_FORMAT_JFIF_GUID;
extern const GUID WPD_OBJECT_FORMAT_PNG_GUID;
extern const GUID WPD_OBJECT_FORMAT_MP3_GUID;
extern const GUID WPD_OBJECT_FORMAT_MPEG_GUID;
extern const GUID WPD_OBJECT_FORMAT_TEXT_GUID;

// STL
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <filesystem>
#include <stdexcept>
#include <set>
#include <unordered_map>

// Windows storage IOCTLs (for deduplication and child caching)
#include <winioctl.h>
#include <setupapi.h>
#pragma comment(lib, "setupapi.lib")

// Lib linkage
#pragma comment(lib, "PortableDeviceGuids.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "comctl32.lib")

// Visual styles (Common Controls 6) — modern buttons, progress bars and the
// Vista-style folder picker used by "Save As".
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_ARM64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='arm64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
