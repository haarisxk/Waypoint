#include "Waypoint.h"

_Function_class_(KSTART_ROUTINE)
_IRQL_requires_same_
static
VOID
WaypointWorkerThread(
    _In_ PVOID Context
)
{
    WDFDEVICE device = (WDFDEVICE)Context;
    PWAYPOINT_DEVICE_CONTEXT deviceContext = WaypointGetDeviceContext(device);
    WAYPOINT_CONFIG config;
    WAYPOINT_COMMAND cmd;
    HID_XFER_PACKET packet;
    UCHAR reportBuffer[WAYPOINT_HID_REPORT_SIZE];
    
    while (!InterlockedCompareExchange(&deviceContext->WorkerShouldStop, 0, 0)) {
        if (deviceContext->SharedMemory == NULL ||
            deviceContext->SharedMemory->Header.State != WAYPOINT_STATE_ACTIVE ||
            (deviceContext->SharedMemory->Header.ReadIndex == deviceContext->SharedMemory->Header.WriteIndex)) {
            
            if (deviceContext->DataEvent) {
                KeWaitForSingleObject(deviceContext->DataEvent, Executive, KernelMode, FALSE, NULL);
            } else {
                WaypointTimingDelay(WAYPOINT_WORKER_IDLE_US, FALSE);
            }
            continue;
        }

        config = deviceContext->SharedMemory->Header.Config;

        // Sanitize config — usermode owns this memory so we can't trust any of it
        if (config.SplitThreshold == 0) {
            config.SplitThreshold = WAYPOINT_DEFAULT_SPLIT_THRESHOLD;
        }
        if (config.PollIntervalUs == 0 || config.PollIntervalUs > 100000) {
            config.PollIntervalUs = WAYPOINT_DEFAULT_POLL_INTERVAL_US;
        }
        if (config.CycleSkipPercent > 100) {
            config.CycleSkipPercent = WAYPOINT_DEFAULT_CYCLE_SKIP_PCT;
        }
        if (config.JitterAmplitude > 50) {
            config.JitterAmplitude = WAYPOINT_DEFAULT_JITTER_AMPLITUDE;
        }

        if (WaypointTimingShouldSkipCycle(config.CycleSkipPercent)) {
            WaypointTimingDelay(config.PollIntervalUs, FALSE);
            continue;
        }

        if (WaypointRingDequeue(deviceContext->SharedMemory, &cmd)) {
            SHORT absDx = (cmd.DeltaX >= 0) ? cmd.DeltaX : -cmd.DeltaX;
            SHORT absDy = (cmd.DeltaY >= 0) ? cmd.DeltaY : -cmd.DeltaY;

            if (cmd.DeltaX == 0 && cmd.DeltaY == 0) {
                WaypointPackMouseReport(reportBuffer, cmd.Buttons, 0, 0, cmd.Wheel);
                packet.reportBuffer = reportBuffer;
                packet.reportBufferLen = WAYPOINT_HID_REPORT_SIZE;
                packet.reportId = 0;
                
                VhfReadReportSubmit(deviceContext->VhfHandle, &packet);
                WaypointTimingDelay(config.PollIntervalUs, config.JitterEnabled != 0);
            }
            else if ((cmd.Flags & WAYPOINT_CMD_FLAG_IMMEDIATE) ||
                (absDx <= (SHORT)config.SplitThreshold && absDy <= (SHORT)config.SplitThreshold)) {
                
                WaypointPackMouseReport(reportBuffer, cmd.Buttons, cmd.DeltaX, cmd.DeltaY, cmd.Wheel);
                packet.reportBuffer = reportBuffer;
                packet.reportBufferLen = WAYPOINT_HID_REPORT_SIZE;
                packet.reportId = 0;
                
                VhfReadReportSubmit(deviceContext->VhfHandle, &packet);
                WaypointTimingDelay(config.PollIntervalUs, config.JitterEnabled != 0);
                
            } else {
                SHORT maxDelta = (absDx > absDy) ? absDx : absDy;
                LONG steps = (maxDelta + config.SplitThreshold - 1) / config.SplitThreshold;
                
                for (LONG i = 0; i < steps; i++) {
                    SHORT subDx = (SHORT)((cmd.DeltaX * (i + 1) / steps) - (cmd.DeltaX * i / steps));
                    SHORT subDy = (SHORT)((cmd.DeltaY * (i + 1) / steps) - (cmd.DeltaY * i / steps));
                    
                    if (config.JitterEnabled) {
                        subDx = WaypointTimingJitter(subDx, config.JitterAmplitude);
                        subDy = WaypointTimingJitter(subDy, config.JitterAmplitude);
                    }
                    
                    WaypointPackMouseReport(reportBuffer, cmd.Buttons, subDx, subDy, (i == steps - 1) ? cmd.Wheel : 0);
                    
                    packet.reportBuffer = reportBuffer;
                    packet.reportBufferLen = WAYPOINT_HID_REPORT_SIZE;
                    packet.reportId = 0;
                    
                    VhfReadReportSubmit(deviceContext->VhfHandle, &packet);
                    WaypointTimingDelay(config.PollIntervalUs, config.JitterEnabled != 0);
                }
            }
        }
    }
    
    PsTerminateSystemThread(STATUS_SUCCESS);
}

NTSTATUS
WaypointWorkerStart(
    _In_ WDFDEVICE                  Device,
    _In_ PWAYPOINT_DEVICE_CONTEXT   DeviceContext
)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    UNICODE_STRING eventName;
    
    InterlockedExchange(&DeviceContext->WorkerShouldStop, FALSE);
    
    RtlInitUnicodeString(&eventName, L"\\BaseNamedObjects\\WaypointDataEvent");
    DeviceContext->DataEvent = IoCreateSynchronizationEvent(&eventName, &DeviceContext->DataEventHandle);
    if (DeviceContext->DataEvent != NULL) {
        KeClearEvent(DeviceContext->DataEvent);
    }
    
    InitializeObjectAttributes(&objAttr, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);
    
    status = PsCreateSystemThread(
        &DeviceContext->WorkerThreadHandle,
        THREAD_ALL_ACCESS,
        &objAttr,
        NULL,
        NULL,
        WaypointWorkerThread,
        (PVOID)Device
    );
    
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    status = ObReferenceObjectByHandle(
        DeviceContext->WorkerThreadHandle,
        THREAD_ALL_ACCESS,
        NULL,
        KernelMode,
        (PVOID*)&DeviceContext->WorkerThreadObject,
        NULL
    );
    
    ZwClose(DeviceContext->WorkerThreadHandle);
    DeviceContext->WorkerThreadHandle = NULL;
    
    return status;
}

VOID
WaypointWorkerStop(
    _Inout_ PWAYPOINT_DEVICE_CONTEXT DeviceContext
)
{
    InterlockedExchange(&DeviceContext->WorkerShouldStop, TRUE);
    
    if (DeviceContext->DataEvent != NULL) {
        KeSetEvent(DeviceContext->DataEvent, 0, FALSE);
    }
    
    if (DeviceContext->WorkerThreadObject != NULL) {
        KeWaitForSingleObject(DeviceContext->WorkerThreadObject, Executive, KernelMode, FALSE, NULL);
        ObDereferenceObject(DeviceContext->WorkerThreadObject);
        DeviceContext->WorkerThreadObject = NULL;
    }
    
    if (DeviceContext->DataEventHandle != NULL) {
        ZwClose(DeviceContext->DataEventHandle);
        DeviceContext->DataEventHandle = NULL;
        DeviceContext->DataEvent = NULL;
    }
}
