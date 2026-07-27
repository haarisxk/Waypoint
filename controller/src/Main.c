// Main.c
// Waypoint User-Mode Controller Demo
//
// I put this together to show how easily a user-mode application can connect to the 
// Waypoint driver and send mouse movements via the lockless shared memory buffer.

#define _USE_MATH_DEFINES

#include <initguid.h>
#include <windows.h>
#include "SharedProtocol.h"
#include <stdio.h>
#include <math.h>

HANDLE g_hDataEvent = NULL;

extern HANDLE WaypointDiscoverDevice(VOID);

BOOLEAN
WaypointConnect(
    _In_ HANDLE hDevice,
    _Outptr_ PWAYPOINT_SHARED_MEMORY* ppShared
    )
{
    WAYPOINT_CONNECT_RESULT result = {0};
    DWORD bytesReturned = 0;

    if (!DeviceIoControl(
            hDevice,
            IOCTL_WAYPOINT_CONNECT,
            NULL,
            0,
            &result,
            sizeof(result),
            &bytesReturned,
            NULL)) {
        wprintf(L"IOCTL_WAYPOINT_CONNECT failed. Error: %lu\n", GetLastError());
        return FALSE;
    }

    *ppShared = (PWAYPOINT_SHARED_MEMORY)result.SharedMemoryAddress;
    wprintf(L"Connected to Waypoint.\n");
    wprintf(L"  Address: %p\n", result.SharedMemoryAddress);
    wprintf(L"  Size: %lu bytes\n", result.SharedMemorySize);
    wprintf(L"  Capacity: %lu entries\n", result.RingCapacity);
    wprintf(L"  Version: %lu\n", result.ProtocolVersion);

    g_hDataEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Global\\WaypointDataEvent");
    if (!g_hDataEvent) {
        wprintf(L"Warning: Failed to open DataEvent (Event-Driven mode disabled). Error: %lu\n", GetLastError());
    }

    return TRUE;
}

VOID
WaypointDisconnect(
    _In_ HANDLE hDevice
    )
{
    DWORD bytesReturned = 0;

    if (!DeviceIoControl(
            hDevice,
            IOCTL_WAYPOINT_DISCONNECT,
            NULL,
            0,
            NULL,
            0,
            &bytesReturned,
            NULL)) {
        wprintf(L"IOCTL_WAYPOINT_DISCONNECT failed. Error: %lu\n", GetLastError());
    } else {
        wprintf(L"Disconnected from Waypoint.\n");
    }

    if (g_hDataEvent) {
        CloseHandle(g_hDataEvent);
        g_hDataEvent = NULL;
    }
}

BOOLEAN
WaypointMoveMouse(
    _Inout_ PWAYPOINT_SHARED_MEMORY pShared,
    _In_ SHORT dx,
    _In_ SHORT dy
    )
{
    WAYPOINT_COMMAND cmd = {0};
    cmd.DeltaX = dx;
    cmd.DeltaY = dy;
    cmd.Buttons = 0;
    cmd.Wheel = 0;
    cmd.Flags = 0;
    cmd.Reserved = 0;

    BOOLEAN success = WaypointRingEnqueue(pShared, &cmd);
    if (success && g_hDataEvent) {
        SetEvent(g_hDataEvent);
    }
    return success;
}

BOOLEAN
WaypointClick(
    _Inout_ PWAYPOINT_SHARED_MEMORY pShared,
    _In_ UCHAR buttons
    )
{
    WAYPOINT_COMMAND cmd = {0};
    cmd.DeltaX = 0;
    cmd.DeltaY = 0;
    cmd.Buttons = buttons;
    cmd.Wheel = 0;
    cmd.Flags = WAYPOINT_CMD_FLAG_BUTTON_EVENT;
    cmd.Reserved = 0;

    BOOLEAN success = WaypointRingEnqueue(pShared, &cmd);
    if (success && g_hDataEvent) {
        SetEvent(g_hDataEvent);
    }
    return success;
}

BOOLEAN
WaypointScroll(
    _Inout_ PWAYPOINT_SHARED_MEMORY pShared,
    _In_ CHAR delta
    )
{
    WAYPOINT_COMMAND cmd = {0};
    cmd.DeltaX = 0;
    cmd.DeltaY = 0;
    cmd.Buttons = 0;
    cmd.Wheel = delta;
    cmd.Flags = WAYPOINT_CMD_FLAG_WHEEL_EVENT;
    cmd.Reserved = 0;

    BOOLEAN success = WaypointRingEnqueue(pShared, &cmd);
    if (success && g_hDataEvent) {
        SetEvent(g_hDataEvent);
    }
    return success;
}

int
wmain(
    _In_ int argc,
    _In_reads_(argc) wchar_t* argv[]
    )
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    wprintf(L"Waypoint HID Controller v0.1.0-beta - Made by Haaris Khan\n");

    HANDLE hDevice = WaypointDiscoverDevice();
    if (hDevice == INVALID_HANDLE_VALUE) {
        wprintf(L"Failed to discover Waypoint device.\n");
        return 1;
    }

    PWAYPOINT_SHARED_MEMORY pShared = NULL;
    if (!WaypointConnect(hDevice, &pShared)) {
        CloseHandle(hDevice);
        return 1;
    }

    wprintf(L"Running smooth circular mouse movement demo...\n");
    
    for (int i = 0; i < 360; ++i) {
        SHORT dx = (SHORT)(cos(i * M_PI / 180.0) * 3.0);
        SHORT dy = (SHORT)(sin(i * M_PI / 180.0) * 3.0);
        
        if (!WaypointMoveMouse(pShared, dx, dy)) {
            wprintf(L"Warning: Buffer full, dropped command at step %d\n", i);
        }
        
        Sleep(5);
    }
    
    wprintf(L"Demo complete. Press Enter to exit...\n");
    getchar();

    WaypointDisconnect(hDevice);
    CloseHandle(hDevice);
    
    return 0;
}
