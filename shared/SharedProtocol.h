// SharedProtocol.h
// 
// This file is extremely important because it is shared between both the kernel driver
// and the user-mode application. It defines exactly how they talk to each other 
// using the shared memory ring buffer, ensuring both sides always agree on the data format!

#pragma once

/* ========================================================================= */
/*  Platform Abstraction                                                     */
/* ========================================================================= */

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <windows.h>
#endif

// Device Interface GUID
// {E7F3A1B2-4C5D-6E7F-8A9B-0C1D2E3F4A5B}
// Our user-mode app uses this unique ID to find the driver when it starts up.

DEFINE_GUID(GUID_DEVINTERFACE_WAYPOINT,
    0xe7f3a1b2, 0x4c5d, 0x6e7f,
    0x8a, 0x9b, 0x0c, 0x1d, 0x2e, 0x3f, 0x4a, 0x5b);

/* ========================================================================= */
/*  IOCTL Codes                                                              */
/* ========================================================================= */

#define FILE_DEVICE_WAYPOINT        0x8000

// IOCTL_WAYPOINT_CONNECT
// Tell the driver we want to connect. The driver will respond by mapping
// the shared ring buffer directly into our process memory space!
#define IOCTL_WAYPOINT_CONNECT \
    CTL_CODE(FILE_DEVICE_WAYPOINT, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

// IOCTL_WAYPOINT_DISCONNECT
// Tells the driver we are done so it can unmap the memory and clean up.
#define IOCTL_WAYPOINT_DISCONNECT \
    CTL_CODE(FILE_DEVICE_WAYPOINT, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* ========================================================================= */
/*  Ring Buffer Configuration                                                */
/* ========================================================================= */

#define WAYPOINT_RING_CAPACITY              4096    /* Must be power of 2   */
#define WAYPOINT_RING_CAPACITY_MASK         (WAYPOINT_RING_CAPACITY - 1)

/* ========================================================================= */
/*  HID Report Limits                                                        */
/* ========================================================================= */

#define WAYPOINT_HID_REPORT_SIZE            6       /* Bytes per report     */
#define WAYPOINT_DELTA_MAX                  32767
#define WAYPOINT_DELTA_MIN                  (-32768)
#define WAYPOINT_WHEEL_MAX                  127
#define WAYPOINT_WHEEL_MIN                  (-127)

/* ========================================================================= */
/*  Default Configuration Values                                             */
/* ========================================================================= */

#define WAYPOINT_DEFAULT_POLL_INTERVAL_US   1000    /* 1 kHz (1000 µs)     */
#define WAYPOINT_DEFAULT_SPLIT_THRESHOLD    12      /* Max units/sub-report */
#define WAYPOINT_DEFAULT_JITTER_AMPLITUDE   1       /* ±1 unit max jitter  */
#define WAYPOINT_DEFAULT_CYCLE_SKIP_PCT     2       /* 2% skip probability */

/* ========================================================================= */
/*  Command Flags                                                            */
/* ========================================================================= */

#define WAYPOINT_CMD_FLAG_NONE              0x00000000UL
#define WAYPOINT_CMD_FLAG_BUTTON_EVENT      0x00000001UL  /* Button changed  */
#define WAYPOINT_CMD_FLAG_WHEEL_EVENT       0x00000002UL  /* Wheel moved     */
#define WAYPOINT_CMD_FLAG_IMMEDIATE         0x00000004UL  /* Skip splitting  */

/* ========================================================================= */
/*  Shared State Values                                                      */
/* ========================================================================= */

#define WAYPOINT_STATE_INACTIVE             0L
#define WAYPOINT_STATE_ACTIVE               1L
#define WAYPOINT_STATE_PAUSED               2L

/* ========================================================================= */
/*  Mouse Button Masks                                                       */
/* ========================================================================= */

#define WAYPOINT_BUTTON_LEFT                0x01
#define WAYPOINT_BUTTON_RIGHT               0x02
#define WAYPOINT_BUTTON_MIDDLE              0x04
#define WAYPOINT_BUTTON_4                   0x08
#define WAYPOINT_BUTTON_5                   0x10
#define WAYPOINT_BUTTON_MASK                0x1F

/* ========================================================================= */
/*  Structures                                                               */
/* ========================================================================= */

// WAYPOINT_COMMAND
// This is exactly what a single command looks like when it goes into the ring buffer.
// It contains our mouse movements and clicks. We pack it tightly to 12 bytes.
#pragma pack(push, 4)
typedef struct _WAYPOINT_COMMAND {
    SHORT   DeltaX;             /* Relative X movement (-32768 .. 32767)    */
    SHORT   DeltaY;             /* Relative Y movement (-32768 .. 32767)    */
    UCHAR   Buttons;            /* Button bitmask (WAYPOINT_BUTTON_xxx)     */
    CHAR    Wheel;              /* Wheel delta   (-127 .. 127)              */
    USHORT  Reserved;           /* Alignment padding (must be zero)         */
    ULONG   Flags;              /* WAYPOINT_CMD_FLAG_xxx                    */
} WAYPOINT_COMMAND, *PWAYPOINT_COMMAND;
#pragma pack(pop)

// WAYPOINT_CONFIG
// Settings we can tweak on the fly from user-mode (like how fast to poll or jitter).
// The driver reads this straight from the shared memory on its next loop.
#pragma pack(push, 4)
typedef struct _WAYPOINT_CONFIG {
    ULONG   PollIntervalUs;     /* Polling interval in microseconds         */
    ULONG   JitterEnabled;      /* Nonzero = enable movement jitter         */
    ULONG   SplitThreshold;     /* Max delta magnitude per sub-report       */
    ULONG   JitterAmplitude;    /* Max jitter offset in units (±)           */
    ULONG   CycleSkipPercent;   /* Probability [0..100] of skipping a cycle */
    ULONG   Reserved[3];        /* Reserved for future use (zero-init)      */
} WAYPOINT_CONFIG, *PWAYPOINT_CONFIG;
#pragma pack(pop)

// WAYPOINT_RING_HEADER
// The main control header at the very start of the shared memory.
// Our user-mode app tells the driver where it is writing via the WriteIndex, 
// and the driver tells us where it's reading via the ReadIndex.
#pragma pack(push, 4)
typedef struct _WAYPOINT_RING_HEADER {
    volatile LONG   WriteIndex;  /* Next write position (user-mode owns)    */
    volatile LONG   ReadIndex;   /* Next read position  (driver owns)       */
    LONG            Capacity;    /* Always WAYPOINT_RING_CAPACITY           */
    volatile LONG   State;       /* WAYPOINT_STATE_xxx                      */
    WAYPOINT_CONFIG Config;      /* Runtime configuration                   */
    ULONG           Version;     /* Protocol version (currently 1)          */
    ULONG           Reserved[3]; /* Reserved for future use (zero-init)     */
} WAYPOINT_RING_HEADER, *PWAYPOINT_RING_HEADER;
#pragma pack(pop)

// WAYPOINT_SHARED_MEMORY
// This is the entire blueprint of the memory region we share between Ring-0 and Ring-3!
typedef struct _WAYPOINT_SHARED_MEMORY {
    WAYPOINT_RING_HEADER    Header;
    WAYPOINT_COMMAND        Entries[WAYPOINT_RING_CAPACITY];
    UCHAR                   Padding[4032]; /* Pad to 69632 bytes (exactly 17 pages) */
} WAYPOINT_SHARED_MEMORY, *PWAYPOINT_SHARED_MEMORY;

// What the driver sends back to us when we successfully connect via IOCTL
typedef struct _WAYPOINT_CONNECT_RESULT {
    PVOID   SharedMemoryAddress; /* User-mode virtual address               */
    ULONG   SharedMemorySize;   /* Total size in bytes                      */
    ULONG   RingCapacity;       /* Number of command entries                */
    ULONG   ProtocolVersion;    /* Must match WAYPOINT_PROTOCOL_VERSION     */
} WAYPOINT_CONNECT_RESULT, *PWAYPOINT_CONNECT_RESULT;

#define WAYPOINT_PROTOCOL_VERSION   1

/* ========================================================================= */
/*  Inline Ring Buffer Operations (User-Mode Only)                           */
/*  Single-producer / single-consumer lock-free ring buffer.                 */
/* ========================================================================= */

#ifndef _KERNEL_MODE

// Drops a new command into the ring buffer for the driver to execute.
// Returns TRUE if it worked, or FALSE if the buffer is currently full.
static __inline BOOLEAN
WaypointRingEnqueue(
    _Inout_ PWAYPOINT_SHARED_MEMORY Shared,
    _In_    const WAYPOINT_COMMAND*  Cmd
)
{
    LONG writeIdx = Shared->Header.WriteIndex;
    LONG nextWrite = (writeIdx + 1) & WAYPOINT_RING_CAPACITY_MASK;

    /* Full when next write position equals read position */
    if (nextWrite == Shared->Header.ReadIndex) {
        return FALSE;
    }

    Shared->Entries[writeIdx] = *Cmd;
    MemoryBarrier();
    InterlockedExchange(&Shared->Header.WriteIndex, nextWrite);
    return TRUE;
}

/*  WaypointRingAvailable
 *  Returns the number of commands available for consumption.
 */
static __inline LONG
WaypointRingAvailable(
    _In_ const WAYPOINT_SHARED_MEMORY* Shared
)
{
    LONG write = Shared->Header.WriteIndex;
    LONG read  = Shared->Header.ReadIndex;
    return (write - read + WAYPOINT_RING_CAPACITY) & WAYPOINT_RING_CAPACITY_MASK;
}

/*  WaypointRingIsEmpty
 *  Returns TRUE if the ring buffer has no pending commands.
 */
static __inline BOOLEAN
WaypointRingIsEmpty(
    _In_ const WAYPOINT_SHARED_MEMORY* Shared
)
{
    return (Shared->Header.ReadIndex == Shared->Header.WriteIndex);
}

#endif /* !_KERNEL_MODE */
