// Discovery.c
// 
// This file handles finding the Waypoint driver dynamically using the SetupDi APIs
// so we don't have to hardcode any device paths.

#include <windows.h>
#include <setupapi.h>
#include <stdio.h>
#include "SharedProtocol.h"

_Success_(return != INVALID_HANDLE_VALUE)
HANDLE
WaypointDiscoverDevice(
    VOID
    )
{
    HDEVINFO hDevInfo;
    SP_DEVICE_INTERFACE_DATA devInterfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA_W pDetailData = NULL;
    DWORD requiredSize = 0;
    HANDLE hDevice = INVALID_HANDLE_VALUE;

    hDevInfo = SetupDiGetClassDevsW(
        &GUID_DEVINTERFACE_WAYPOINT,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
        );

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        wprintf(L"SetupDiGetClassDevsW failed. Error: %lu\n", GetLastError());
        return INVALID_HANDLE_VALUE;
    }

    devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    if (!SetupDiEnumDeviceInterfaces(
            hDevInfo,
            NULL,
            &GUID_DEVINTERFACE_WAYPOINT,
            0,
            &devInterfaceData)) {
        wprintf(L"SetupDiEnumDeviceInterfaces failed (no devices found). Error: %lu\n", GetLastError());
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return INVALID_HANDLE_VALUE;
    }

    SetupDiGetDeviceInterfaceDetailW(
        hDevInfo,
        &devInterfaceData,
        NULL,
        0,
        &requiredSize,
        NULL
        );

    if (requiredSize == 0) {
        wprintf(L"SetupDiGetDeviceInterfaceDetailW failed to return required size. Error: %lu\n", GetLastError());
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return INVALID_HANDLE_VALUE;
    }

    pDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, requiredSize);
    if (!pDetailData) {
        wprintf(L"Failed to allocate memory for device detail data.\n");
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return INVALID_HANDLE_VALUE;
    }

    pDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

    if (!SetupDiGetDeviceInterfaceDetailW(
            hDevInfo,
            &devInterfaceData,
            pDetailData,
            requiredSize,
            NULL,
            NULL)) {
        wprintf(L"SetupDiGetDeviceInterfaceDetailW failed. Error: %lu\n", GetLastError());
        HeapFree(GetProcessHeap(), 0, pDetailData);
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return INVALID_HANDLE_VALUE;
    }

    hDevice = CreateFileW(
        pDetailData->DevicePath,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
        );

    if (hDevice == INVALID_HANDLE_VALUE) {
        wprintf(L"CreateFileW failed on path %ws. Error: %lu\n", pDetailData->DevicePath, GetLastError());
    } else {
        wprintf(L"Successfully opened device: %ws\n", pDetailData->DevicePath);
    }

    HeapFree(GetProcessHeap(), 0, pDetailData);
    SetupDiDestroyDeviceInfoList(hDevInfo);

    return hDevice;
}
