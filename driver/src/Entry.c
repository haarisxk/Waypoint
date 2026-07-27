// Entry.c
//
// This is the absolute starting point of our driver. When Windows loads Waypoint,
// this is where we wake up, initialize everything, and start talking to the system.

#include <initguid.h>
#include "Waypoint.h"

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;

    WDF_DRIVER_CONFIG_INIT(&config, WaypointEvtDeviceAdd);

    status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);

    return status;
}

NTSTATUS
WaypointEvtDeviceAdd(
    _In_    WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    NTSTATUS status;
    WDF_FILEOBJECT_CONFIG fileConfig;
    WDF_OBJECT_ATTRIBUTES fileAttr;
    WDF_OBJECT_ATTRIBUTES deviceAttr;
    WDFDEVICE device;
    PWAYPOINT_DEVICE_CONTEXT deviceContext;
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDFQUEUE queue;
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;

    UNREFERENCED_PARAMETER(Driver);

    WDF_FILEOBJECT_CONFIG_INIT(
        &fileConfig,
        WaypointEvtDeviceFileCreate,
        NULL,
        WaypointEvtFileCleanup
    );

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&fileAttr, WAYPOINT_FILE_CONTEXT);
    WdfDeviceInitSetFileObjectConfig(DeviceInit, &fileConfig, &fileAttr);

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDeviceSelfManagedIoCleanup = WaypointEvtSelfManagedIoCleanup;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttr, WAYPOINT_DEVICE_CONTEXT);
    
    KdPrint(("Waypoint: EvtDeviceAdd started\n"));

    status = WdfDeviceCreate(&DeviceInit, &deviceAttr, &device);
    if (!NT_SUCCESS(status)) {
        KdPrint(("Waypoint: WdfDeviceCreate failed 0x%x\n", status));
        return status;
    }

    deviceContext = WaypointGetDeviceContext(device);
    RtlZeroMemory(deviceContext, sizeof(WAYPOINT_DEVICE_CONTEXT));

    WaypointIdentityGenerate(deviceContext);

    status = WaypointDeviceCreateVhf(device, deviceContext);
    if (!NT_SUCCESS(status)) {
        KdPrint(("Waypoint: WaypointDeviceCreateVhf failed 0x%x\n", status));
        WaypointSharedMemoryDestroy(deviceContext);
        return status;
    }

    status = WaypointSharedMemoryCreate(deviceContext);
    if (!NT_SUCCESS(status)) {
        KdPrint(("Waypoint: WaypointSharedMemoryCreate failed 0x%x\n", status));
        WaypointDeviceDestroyVhf(deviceContext);
        WaypointSharedMemoryDestroy(deviceContext);
        return status;
    }

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);
    queueConfig.EvtIoDeviceControl = WaypointEvtIoDeviceControl;

    status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);
    if (!NT_SUCCESS(status)) {
        KdPrint(("Waypoint: WdfIoQueueCreate failed 0x%x\n", status));
        WaypointDeviceDestroyVhf(deviceContext);
        WaypointSharedMemoryDestroy(deviceContext);
        return status;
    }

    status = WdfDeviceCreateDeviceInterface(device, &GUID_DEVINTERFACE_WAYPOINT, NULL);
    if (!NT_SUCCESS(status)) {
        KdPrint(("Waypoint: WdfDeviceCreateDeviceInterface failed 0x%x\n", status));
        WaypointDeviceDestroyVhf(deviceContext);
        WaypointSharedMemoryDestroy(deviceContext);
        return status;
    }

    status = WaypointWorkerStart(device, deviceContext);
    if (!NT_SUCCESS(status)) {
        KdPrint(("Waypoint: WaypointWorkerStart failed 0x%x\n", status));
        WaypointDeviceDestroyVhf(deviceContext);
        WaypointSharedMemoryDestroy(deviceContext);
        return status;
    }

    return STATUS_SUCCESS;
}

VOID
WaypointEvtSelfManagedIoCleanup(
    _In_ WDFDEVICE Device
)
{
    PWAYPOINT_DEVICE_CONTEXT deviceContext = WaypointGetDeviceContext(Device);

    WaypointWorkerStop(deviceContext);
    WaypointDeviceDestroyVhf(deviceContext);

    if (deviceContext->ClientConnected && deviceContext->ClientMappedAddress != NULL) {
        WaypointSharedMemoryUnmapForClient(deviceContext, deviceContext->ClientMappedAddress);
    }

    WaypointSharedMemoryDestroy(deviceContext);
}

VOID
WaypointEvtDeviceFileCreate(
    _In_ WDFDEVICE     Device,
    _In_ WDFREQUEST    Request,
    _In_ WDFFILEOBJECT FileObject
)
{
    PWAYPOINT_FILE_CONTEXT fileContext;

    UNREFERENCED_PARAMETER(Device);

    fileContext = WaypointGetFileContext(FileObject);
    fileContext->IsConnected = FALSE;
    fileContext->MappedAddress = NULL;

    WdfRequestComplete(Request, STATUS_SUCCESS);
}

VOID
WaypointEvtFileCleanup(
    _In_ WDFFILEOBJECT FileObject
)
{
    PWAYPOINT_FILE_CONTEXT fileContext = WaypointGetFileContext(FileObject);
    WDFDEVICE device = WdfFileObjectGetDevice(FileObject);
    PWAYPOINT_DEVICE_CONTEXT deviceContext = WaypointGetDeviceContext(device);

    if (fileContext->IsConnected && fileContext->MappedAddress != NULL) {
        WaypointSharedMemoryUnmapForClient(deviceContext, fileContext->MappedAddress);
        fileContext->IsConnected = FALSE;
        fileContext->MappedAddress = NULL;
    }
}

VOID
WaypointEvtIoDeviceControl(
    _In_ WDFQUEUE   Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t     OutputBufferLength,
    _In_ size_t     InputBufferLength,
    _In_ ULONG      IoControlCode
)
{
    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    PWAYPOINT_DEVICE_CONTEXT deviceContext = WaypointGetDeviceContext(device);
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG_PTR bytesReturned = 0;
    WDFFILEOBJECT fileObject;
    PWAYPOINT_FILE_CONTEXT fileContext;

    UNREFERENCED_PARAMETER(InputBufferLength);

    switch (IoControlCode) {
        case IOCTL_WAYPOINT_CONNECT:
        {
            PWAYPOINT_CONNECT_RESULT connectResult;
            PVOID mappedAddress = NULL;
            ULONG mappedSize = 0;

            fileObject = WdfRequestGetFileObject(Request);
            fileContext = WaypointGetFileContext(fileObject);

            if (fileContext->IsConnected) {
                status = STATUS_ALREADY_COMMITTED;
                break;
            }

            if (OutputBufferLength < sizeof(WAYPOINT_CONNECT_RESULT)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            status = WdfRequestRetrieveOutputBuffer(Request, sizeof(WAYPOINT_CONNECT_RESULT), (PVOID*)&connectResult, NULL);
            if (!NT_SUCCESS(status)) {
                break;
            }

            status = WaypointSharedMemoryMapForClient(deviceContext, &mappedAddress, &mappedSize);
            if (NT_SUCCESS(status)) {
                connectResult->SharedMemoryAddress = mappedAddress;
                connectResult->SharedMemorySize = mappedSize;
                connectResult->RingCapacity = WAYPOINT_RING_CAPACITY;
                connectResult->ProtocolVersion = WAYPOINT_PROTOCOL_VERSION;

                fileContext->IsConnected = TRUE;
                fileContext->MappedAddress = mappedAddress;

                bytesReturned = sizeof(WAYPOINT_CONNECT_RESULT);
            }
            break;
        }

        case IOCTL_WAYPOINT_DISCONNECT:
        {
            fileObject = WdfRequestGetFileObject(Request);
            fileContext = WaypointGetFileContext(fileObject);

            if (!fileContext->IsConnected) {
                status = STATUS_INVALID_DEVICE_STATE;
                break;
            }

            WaypointSharedMemoryUnmapForClient(deviceContext, fileContext->MappedAddress);

            fileContext->IsConnected = FALSE;
            fileContext->MappedAddress = NULL;

            status = STATUS_SUCCESS;
            break;
        }

        default:
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }

    WdfRequestCompleteWithInformation(Request, status, bytesReturned);
}
