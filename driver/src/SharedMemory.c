// SharedMemory.c
//
// Allocates the NonPaged ring buffer, maps it into usermode via MDL, and handles
// cleanup when the client disconnects or the driver unloads.

#include "Waypoint.h"

NTSTATUS
WaypointSharedMemoryCreate(
    _Inout_ PWAYPOINT_DEVICE_CONTEXT DeviceContext
)
{
    PWAYPOINT_SHARED_MEMORY sharedMemory = NULL;
    PMDL mdl = NULL;
    PHYSICAL_ADDRESS lowAddress, highAddress, skipBytes;

    lowAddress.QuadPart = 0;
    highAddress.QuadPart = -1;
    skipBytes.QuadPart = 0;

    mdl = MmAllocatePagesForMdl(lowAddress, highAddress, skipBytes, sizeof(WAYPOINT_SHARED_MEMORY));
    if (mdl == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    sharedMemory = (PWAYPOINT_SHARED_MEMORY)MmGetSystemAddressForMdlSafe(mdl, NormalPagePriority);
    if (sharedMemory == NULL) {
        MmFreePagesFromMdl(mdl);
        IoFreeMdl(mdl);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(sharedMemory, sizeof(WAYPOINT_SHARED_MEMORY));

    sharedMemory->Header.Capacity = WAYPOINT_RING_CAPACITY;
    sharedMemory->Header.State = WAYPOINT_STATE_INACTIVE;
    sharedMemory->Header.Version = WAYPOINT_PROTOCOL_VERSION;

    sharedMemory->Header.Config.PollIntervalUs = WAYPOINT_DEFAULT_POLL_INTERVAL_US;
    sharedMemory->Header.Config.JitterEnabled = 1;
    sharedMemory->Header.Config.SplitThreshold = WAYPOINT_DEFAULT_SPLIT_THRESHOLD;
    sharedMemory->Header.Config.JitterAmplitude = WAYPOINT_DEFAULT_JITTER_AMPLITUDE;
    sharedMemory->Header.Config.CycleSkipPercent = WAYPOINT_DEFAULT_CYCLE_SKIP_PCT;

    DeviceContext->SharedMemory = sharedMemory;
    DeviceContext->SharedMemoryMdl = mdl;
    DeviceContext->SharedMemorySize = sizeof(WAYPOINT_SHARED_MEMORY);

    return STATUS_SUCCESS;
}

VOID
WaypointSharedMemoryDestroy(
    _Inout_ PWAYPOINT_DEVICE_CONTEXT DeviceContext
)
{
    if (DeviceContext->SharedMemoryMdl != NULL) {
        if (DeviceContext->SharedMemory != NULL) {
            MmUnmapLockedPages(DeviceContext->SharedMemory, DeviceContext->SharedMemoryMdl);
            DeviceContext->SharedMemory = NULL;
        }
        MmFreePagesFromMdl(DeviceContext->SharedMemoryMdl);
        IoFreeMdl(DeviceContext->SharedMemoryMdl);
        DeviceContext->SharedMemoryMdl = NULL;
    }
    DeviceContext->SharedMemorySize = 0;
}


NTSTATUS
WaypointSharedMemoryMapForClient(
    _Inout_ PWAYPOINT_DEVICE_CONTEXT DeviceContext,
    _Out_   PVOID*  MappedAddress,
    _Out_   PULONG  MappedSize
)
{
    PVOID userVa = NULL;
    
    if (InterlockedCompareExchange(&DeviceContext->ClientConnected, TRUE, FALSE) == TRUE) {
        return STATUS_DEVICE_BUSY;
    }

    __try {
        userVa = MmMapLockedPagesSpecifyCache(DeviceContext->SharedMemoryMdl, UserMode, MmCached, NULL, FALSE, NormalPagePriority);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        userVa = NULL;
    }

    if (userVa == NULL) {
        InterlockedExchange(&DeviceContext->ClientConnected, FALSE);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    DeviceContext->ClientProcess = PsGetCurrentProcess();
    ObReferenceObject(DeviceContext->ClientProcess);

    DeviceContext->ClientMappedAddress = userVa;
    DeviceContext->SharedMemory->Header.State = WAYPOINT_STATE_ACTIVE;

    *MappedAddress = userVa;
    *MappedSize = DeviceContext->SharedMemorySize;

    return STATUS_SUCCESS;
}

VOID
WaypointSharedMemoryUnmapForClient(
    _Inout_ PWAYPOINT_DEVICE_CONTEXT DeviceContext,
    _In_    PVOID   MappedAddress
)
{
    if (!DeviceContext->ClientConnected) {
        return;
    }

    DeviceContext->SharedMemory->Header.State = WAYPOINT_STATE_INACTIVE;
    DeviceContext->SharedMemory->Header.WriteIndex = 0;
    DeviceContext->SharedMemory->Header.ReadIndex = 0;

    if (PsGetCurrentProcess() == DeviceContext->ClientProcess) {
        __try {
            MmUnmapLockedPages(MappedAddress, DeviceContext->SharedMemoryMdl);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            // Ignored
        }
    }

    if (DeviceContext->ClientProcess != NULL) {
        ObDereferenceObject(DeviceContext->ClientProcess);
        DeviceContext->ClientProcess = NULL;
    }

    DeviceContext->ClientMappedAddress = NULL;
    InterlockedExchange(&DeviceContext->ClientConnected, FALSE);
}
