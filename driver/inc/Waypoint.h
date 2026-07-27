// Waypoint.h
// 
// This is the main internal header for the kernel driver. It defines our context
// structures, constants, and all the function prototypes. Note that user-mode 
// applications should NOT include this file—they should use SharedProtocol.h instead.

#pragma once

// WDK and Framework Headers

#include <ntddk.h>
#include <wdf.h>
#include <vhf.h>
#include <ntstrsafe.h>

/* Include shared protocol definitions */
#include "SharedProtocol.h"

// Pool Tag (We use 'Wypt' in little-endian for all our memory allocations)

#define WAYPOINT_POOL_TAG           'tpyW'    /* 'Wypt' in little-endian    */

// Limits for spoofing our hardware identity strings

#define WAYPOINT_MAX_SERIAL_LEN     32
#define WAYPOINT_MAX_PRODUCT_LEN    64
#define WAYPOINT_MAX_MFGR_LEN      32

// How many different hardware identity presets we have available to pick from

#define WAYPOINT_IDENTITY_COUNT     8         /* Number of identity presets  */

// Timing Constants

// KeDelayExecutionThread uses 100-ns units, so this macro converts microseconds for us
#define WAYPOINT_US_TO_100NS(us)    (-(LONGLONG)(us) * 10LL)

// How long the worker thread sleeps (in µs) while waiting for new commands
#define WAYPOINT_WORKER_IDLE_US     100

// Device Context
// We only ever have one instance of this per WDFDEVICE. It holds all our runtime state.

typedef struct _WAYPOINT_DEVICE_CONTEXT {

    /* --- VHF Handle --- */
    VHFHANDLE                   VhfHandle;

    /* --- Hardware Identity (randomized per boot) --- */
    USHORT                      VendorId;
    USHORT                      ProductId;
    USHORT                      VersionNumber;
    WCHAR                       SerialNumber[WAYPOINT_MAX_SERIAL_LEN];
    WCHAR                       ProductName[WAYPOINT_MAX_PRODUCT_LEN];
    WCHAR                       Manufacturer[WAYPOINT_MAX_MFGR_LEN];

    /* --- Shared Memory (NonPaged pool ring buffer) --- */
    PWAYPOINT_SHARED_MEMORY     SharedMemory;       /* Kernel VA             */
    PMDL                        SharedMemoryMdl;    /* MDL for user mapping  */
    ULONG                       SharedMemorySize;

    /* --- Worker Thread --- */
    HANDLE                      WorkerThreadHandle;
    PKTHREAD                    WorkerThreadObject;
    volatile LONG               WorkerShouldStop;

    /* --- Synchronization --- */
    HANDLE                      DataEventHandle;
    PKEVENT                     DataEvent;

    /* --- Client Tracking (single client at a time) --- */
    volatile LONG               ClientConnected;
    PVOID                       ClientMappedAddress; /* User VA              */
    PEPROCESS                   ClientProcess;

} WAYPOINT_DEVICE_CONTEXT, *PWAYPOINT_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(WAYPOINT_DEVICE_CONTEXT, WaypointGetDeviceContext)

// File Context
// This tracks whether a specific file handle currently has an active memory mapping.

typedef struct _WAYPOINT_FILE_CONTEXT {
    BOOLEAN     IsConnected;
    PVOID       MappedAddress;      /* User VA returned by MmMapLockedPages */
} WAYPOINT_FILE_CONTEXT, *PWAYPOINT_FILE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(WAYPOINT_FILE_CONTEXT, WaypointGetFileContext)

/* ========================================================================= */
/*  Function Prototypes — Entry.c (Driver lifecycle & IOCTL dispatch)        */
/* ========================================================================= */

DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD                   WaypointEvtDeviceAdd;
EVT_WDF_DEVICE_SELF_MANAGED_IO_CLEANUP      WaypointEvtSelfManagedIoCleanup;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL          WaypointEvtIoDeviceControl;
EVT_WDF_DEVICE_FILE_CREATE                  WaypointEvtDeviceFileCreate;
EVT_WDF_FILE_CLEANUP                        WaypointEvtFileCleanup;

/* ========================================================================= */
/*  Function Prototypes — Device.c (VHF creation & device interface)         */
/* ========================================================================= */

NTSTATUS
WaypointDeviceCreateVhf(
    _In_    WDFDEVICE                   Device,
    _Inout_ PWAYPOINT_DEVICE_CONTEXT    DeviceContext
);

VOID
WaypointDeviceDestroyVhf(
    _Inout_ PWAYPOINT_DEVICE_CONTEXT    DeviceContext
);

/* ========================================================================= */
/*  Function Prototypes — Descriptor.c (HID report descriptor & packing)     */
/* ========================================================================= */

_Must_inspect_result_
PUCHAR
WaypointGetReportDescriptor(
    _Out_ PUSHORT DescriptorLength
);

VOID
WaypointPackMouseReport(
    _Out_writes_bytes_(WAYPOINT_HID_REPORT_SIZE) PUCHAR ReportBuffer,
    _In_ UCHAR  Buttons,
    _In_ SHORT  DeltaX,
    _In_ SHORT  DeltaY,
    _In_ CHAR   Wheel
);

/* ========================================================================= */
/*  Function Prototypes — Identity.c (Hardware identity randomization)       */
/* ========================================================================= */

VOID
WaypointIdentityGenerate(
    _Inout_ PWAYPOINT_DEVICE_CONTEXT DeviceContext
);

/* ========================================================================= */
/*  Function Prototypes — SharedMemory.c (Ring buffer allocation & mapping)  */
/* ========================================================================= */

NTSTATUS
WaypointSharedMemoryCreate(
    _Inout_ PWAYPOINT_DEVICE_CONTEXT DeviceContext
);

VOID
WaypointSharedMemoryDestroy(
    _Inout_ PWAYPOINT_DEVICE_CONTEXT DeviceContext
);

NTSTATUS
WaypointSharedMemoryMapForClient(
    _Inout_ PWAYPOINT_DEVICE_CONTEXT DeviceContext,
    _Out_   PVOID*  MappedAddress,
    _Out_   PULONG  MappedSize
);

VOID
WaypointSharedMemoryUnmapForClient(
    _Inout_ PWAYPOINT_DEVICE_CONTEXT DeviceContext,
    _In_    PVOID   MappedAddress
);

/* ========================================================================= */
/*  Function Prototypes — Worker.c (System worker thread)                    */
/* ========================================================================= */

NTSTATUS
WaypointWorkerStart(
    _In_ WDFDEVICE                  Device,
    _In_ PWAYPOINT_DEVICE_CONTEXT   DeviceContext
);

VOID
WaypointWorkerStop(
    _Inout_ PWAYPOINT_DEVICE_CONTEXT DeviceContext
);

/* ========================================================================= */
/*  Function Prototypes — Timing.c (High-resolution delay & entropy)         */
/* ========================================================================= */

VOID
WaypointTimingDelay(
    _In_ ULONG   BaseIntervalUs,
    _In_ BOOLEAN ApplyJitter
);

BOOLEAN
WaypointTimingShouldSkipCycle(
    _In_ ULONG SkipPercentage
);

ULONG
WaypointTimingRandom(
    _In_ ULONG MinValue,
    _In_ ULONG MaxValue
);

SHORT
WaypointTimingJitter(
    _In_ SHORT  Value,
    _In_ ULONG  Amplitude
);

// Inline Helpers

// Pulls the next command out of the shared ring buffer so the driver can process it.
// Returns TRUE if we got a command, or FALSE if the buffer is currently empty.
static __forceinline BOOLEAN
WaypointRingDequeue(
    _Inout_ PWAYPOINT_SHARED_MEMORY Shared,
    _Out_   PWAYPOINT_COMMAND       Cmd
)
{
    // Never trust usermode memory! Mask the indexes immediately.
    LONG readIdx = Shared->Header.ReadIndex & WAYPOINT_RING_CAPACITY_MASK;
    LONG writeIdx = Shared->Header.WriteIndex & WAYPOINT_RING_CAPACITY_MASK;

    if (readIdx == writeIdx) {
        return FALSE;
    }

    *Cmd = Shared->Entries[readIdx];
    MemoryBarrier();
    InterlockedExchange(&Shared->Header.ReadIndex,
                        (readIdx + 1) & WAYPOINT_RING_CAPACITY_MASK);
    return TRUE;
}
