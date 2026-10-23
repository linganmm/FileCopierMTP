// WpdGuids.cpp
// ─────────────────────────────────────────────────────────────────────────────
// Defines all WPD GUIDs that are missing or unreliable across SDK versions.
//
// INITGUID must be defined before including any WPD header so that
// DEFINE_GUID expands to an actual object definition (not just a declaration).
// This file must NOT be compiled with a precompiled header that already
// included those headers without INITGUID — so we do NOT include pch.h here.
// ─────────────────────────────────────────────────────────────────────────────

#define INITGUID
#include <windows.h>
#include <portabledeviceapi.h>
#include <portabledevicetypes.h>
#include <portabledevice.h>

// ── Content types ─────────────────────────────────────────────────────────────

// WPD_CONTENT_TYPE_FOLDER  {27E2E392-A111-48E0-AB0C-E17705A05F85}
const GUID WPD_CONTENT_TYPE_FOLDER_GUID =
    { 0x27E2E392, 0xA111, 0x48E0, { 0xAB, 0x0C, 0xE1, 0x77, 0x05, 0xA0, 0x5F, 0x85 } };

// WPD_CONTENT_TYPE_FUNCTIONAL_OBJECT  {99ED0160-17FF-4C44-9D98-1D7A6F941921}
const GUID WPD_CONTENT_TYPE_FUNCTIONAL_OBJECT_GUID =
    { 0x99ED0160, 0x17FF, 0x4C44, { 0x9D, 0x98, 0x1D, 0x7A, 0x6F, 0x94, 0x19, 0x21 } };

// WPD_CONTENT_TYPE_STORAGE  {23F05BBC-15DE-4C2A-A55B-A9AF5CE412EF}
const GUID WPD_CONTENT_TYPE_STORAGE_GUID =
    { 0x23F05BBC, 0x15DE, 0x4C2A, { 0xA5, 0x5B, 0xA9, 0xAF, 0x5C, 0xE4, 0x12, 0xEF } };

// WPD_CONTENT_TYPE_GENERIC_FILE  {0085E0A6-8D34-45D7-BC5C-447E59C73D48}
const GUID WPD_CONTENT_TYPE_GENERIC_FILE_GUID =
    { 0x0085E0A6, 0x8D34, 0x45D7, { 0xBC, 0x5C, 0x44, 0x7E, 0x59, 0xC7, 0x3D, 0x48 } };

// WPD_CONTENT_TYPE_ALL  {80E170D2-1055-4A3E-B952-82CC4F8A8689}
const GUID WPD_CONTENT_TYPE_ALL_GUID =
    { 0x80E170D2, 0x1055, 0x4A3E, { 0xB9, 0x52, 0x82, 0xCC, 0x4F, 0x8A, 0x86, 0x89 } };

// ── Object formats ────────────────────────────────────────────────────────────

// WPD_OBJECT_FORMAT_UNSPECIFIED  {30010000-AE6C-4804-98BA-C57B46965FE7}
const GUID WPD_OBJECT_FORMAT_UNSPECIFIED_GUID =
    { 0x30010000, 0xAE6C, 0x4804, { 0x98, 0xBA, 0xC5, 0x7B, 0x46, 0x96, 0x5F, 0xE7 } };

// WPD_OBJECT_FORMAT_PROPERTIES_ONLY  {30000000-AE6C-4804-98BA-C57B46965FE7}
const GUID WPD_OBJECT_FORMAT_PROPERTIES_ONLY_GUID =
    { 0x30000000, 0xAE6C, 0x4804, { 0x98, 0xBA, 0xC5, 0x7B, 0x46, 0x96, 0x5F, 0xE7 } };

// WPD_OBJECT_FORMAT_JFIF (JPEG)  {3010000A-AE6C-4804-98BA-C57B46965FE7}
const GUID WPD_OBJECT_FORMAT_JFIF_GUID =
    { 0x3010000A, 0xAE6C, 0x4804, { 0x98, 0xBA, 0xC5, 0x7B, 0x46, 0x96, 0x5F, 0xE7 } };

// WPD_OBJECT_FORMAT_PNG  {3010000C-AE6C-4804-98BA-C57B46965FE7}
const GUID WPD_OBJECT_FORMAT_PNG_GUID =
    { 0x3010000C, 0xAE6C, 0x4804, { 0x98, 0xBA, 0xC5, 0x7B, 0x46, 0x96, 0x5F, 0xE7 } };

// WPD_OBJECT_FORMAT_MP3  {30090000-AE6C-4804-98BA-C57B46965FE7}
const GUID WPD_OBJECT_FORMAT_MP3_GUID =
    { 0x30090000, 0xAE6C, 0x4804, { 0x98, 0xBA, 0xC5, 0x7B, 0x46, 0x96, 0x5F, 0xE7 } };

// WPD_OBJECT_FORMAT_MPEG (MP4)  {300B0000-AE6C-4804-98BA-C57B46965FE7}
const GUID WPD_OBJECT_FORMAT_MPEG_GUID =
    { 0x300B0000, 0xAE6C, 0x4804, { 0x98, 0xBA, 0xC5, 0x7B, 0x46, 0x96, 0x5F, 0xE7 } };

// WPD_OBJECT_FORMAT_TEXT  {30030000-AE6C-4804-98BA-C57B46965FE7}
const GUID WPD_OBJECT_FORMAT_TEXT_GUID =
    { 0x30030000, 0xAE6C, 0x4804, { 0x98, 0xBA, 0xC5, 0x7B, 0x46, 0x96, 0x5F, 0xE7 } };
