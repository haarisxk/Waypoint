// Device.c
//
// Handles the actual creation and setup of our virtual HID device using the Virtual HID Framework (VHF).

#include "Waypoint.h"

NTSTATUS
WaypointDeviceCreateVhf(
    _In_    WDFDEVICE                   Device,
    _Inout_ PWAYPOINT_DEVICE_CONTEXT    DeviceContext
)
{
    NTSTATUS status;
    VHF_CONFIG vhfConfig;
    USHORT length = 0;
    PUCHAR descriptor;

    descriptor = WaypointGetReportDescriptor(&length);

    VHF_CONFIG_INIT(&vhfConfig, WdfDeviceWdmGetDeviceObject(Device), length, descriptor);

    vhfConfig.VendorID = DeviceContext->VendorId;
    vhfConfig.ProductID = DeviceContext->ProductId;
    vhfConfig.VersionNumber = DeviceContext->VersionNumber;

    status = VhfCreate(&vhfConfig, &DeviceContext->VhfHandle);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = VhfStart(DeviceContext->VhfHandle);
    if (!NT_SUCCESS(status)) {
        VhfDelete(DeviceContext->VhfHandle, TRUE);
        DeviceContext->VhfHandle = NULL;
    }

    return status;
}

VOID
WaypointDeviceDestroyVhf(
    _Inout_ PWAYPOINT_DEVICE_CONTEXT    DeviceContext
)
{
    if (DeviceContext->VhfHandle != NULL) {
        VhfDelete(DeviceContext->VhfHandle, TRUE);
        DeviceContext->VhfHandle = NULL;
    }
}
