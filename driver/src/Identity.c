// Identity.c
//
// Picks a random mouse brand preset on each boot and generates a unique serial
// number so the driver doesn't show up as a generic 0x0000 device in Device Manager.

#include "Waypoint.h"
#include <ntstrsafe.h>

typedef struct _WAYPOINT_IDENTITY_PRESET {
    USHORT VendorId;
    USHORT ProductId;
    USHORT BaseVersion;
    PCWSTR Manufacturer;
    PCWSTR ProductName;
} WAYPOINT_IDENTITY_PRESET, *PWAYPOINT_IDENTITY_PRESET;

static const WAYPOINT_IDENTITY_PRESET g_IdentityPresets[WAYPOINT_IDENTITY_COUNT] = {
    { 0x046D, 0xC07E, 0x0100, L"Logitech",    L"Logitech G402" },
    { 0x046D, 0xC08B, 0x0100, L"Logitech",    L"Logitech G502 HERO" },
    { 0x046D, 0xC084, 0x0100, L"Logitech",    L"Logitech G203" },
    { 0x046D, 0xC083, 0x0100, L"Logitech",    L"Logitech G403" },
    { 0x1532, 0x0078, 0x0100, L"Razer",       L"Razer DeathAdder V2" },
    { 0x1532, 0x0084, 0x0100, L"Razer",       L"Razer Viper Mini" },
    { 0x1038, 0x1832, 0x0100, L"SteelSeries", L"SteelSeries Rival 3" },
    { 0x1B1C, 0x1B75, 0x0100, L"Corsair",     L"Corsair HARPOON" }
};

VOID
WaypointIdentityGenerate(
    _Inout_ PWAYPOINT_DEVICE_CONTEXT DeviceContext
)
{
    LARGE_INTEGER pc;
    ULONG entropy;
    ULONG presetIdx;
    const WAYPOINT_IDENTITY_PRESET* preset;
    ULONG serialEntropy;
    USHORT versionVariation;

    KeQueryPerformanceCounter(&pc);
    entropy = pc.LowPart;

    /* Select a random preset */
    presetIdx = entropy % WAYPOINT_IDENTITY_COUNT;
    preset = &g_IdentityPresets[presetIdx];

    DeviceContext->VendorId = preset->VendorId;
    DeviceContext->ProductId = preset->ProductId;

    /* Generate a version number: base version +- small random variation (0-5) */
    versionVariation = (USHORT)((entropy >> 4) % 6);
    if ((entropy >> 8) & 1) {
        DeviceContext->VersionNumber = preset->BaseVersion + versionVariation;
    } else {
        DeviceContext->VersionNumber = preset->BaseVersion - versionVariation;
    }

    /* Generate a serial number: 8 uppercase hex characters */
    serialEntropy = (entropy >> 16) ^ pc.HighPart;
    RtlStringCchPrintfW(DeviceContext->SerialNumber,
                        WAYPOINT_MAX_SERIAL_LEN,
                        L"WP%08X",
                        serialEntropy);

    /* Copy strings */
    RtlStringCchCopyW(DeviceContext->Manufacturer,
                      WAYPOINT_MAX_MFGR_LEN,
                      preset->Manufacturer);

    RtlStringCchCopyW(DeviceContext->ProductName,
                      WAYPOINT_MAX_PRODUCT_LEN,
                      preset->ProductName);
}
