// Timing.c
//
// High-res delay and entropy helpers. The LCG random number generator handles
// timing jitter and cycle skipping so input patterns don't look robotic.

#include "Waypoint.h"

static volatile LONG g_TimingSeed = 0;

ULONG
WaypointTimingRandom(
    _In_ ULONG MinValue,
    _In_ ULONG MaxValue
)
{
    LONG currentSeed = g_TimingSeed;
    LONG newSeed;
    ULONG randVal;

    /* Initialize seed on first use */
    if (currentSeed == 0) {
        LARGE_INTEGER pc;
        KeQueryPerformanceCounter(&pc);
        
        newSeed = (LONG)pc.LowPart;
        if (newSeed == 0) {
            newSeed = 1;
        }
        
        InterlockedCompareExchange(&g_TimingSeed, newSeed, 0);
        currentSeed = g_TimingSeed;
    }

    /* Simple LCG */
    currentSeed = currentSeed * 1103515245 + 12345;
    InterlockedExchange(&g_TimingSeed, currentSeed);

    randVal = (ULONG)((currentSeed >> 16) & 0x7FFF);

    if (MaxValue <= MinValue) {
        return MinValue;
    }

    return MinValue + (randVal % (MaxValue - MinValue + 1));
}

VOID
WaypointTimingDelay(
    _In_ ULONG BaseIntervalUs,
    _In_ BOOLEAN ApplyJitter
)
{
    ULONG delayUs = BaseIntervalUs;
    LARGE_INTEGER interval;

    if (ApplyJitter) {
        /* Add random +/- 3 us variation */
        ULONG jitter = WaypointTimingRandom(0, 6);
        
        if (jitter < 3) {
            ULONG sub = 3 - jitter;
            if (delayUs > sub) {
                delayUs -= sub;
            } else {
                delayUs = 0;
            }
        } else {
            delayUs += (jitter - 3);
        }
    }

    interval.QuadPart = WAYPOINT_US_TO_100NS(delayUs);
    KeDelayExecutionThread(KernelMode, FALSE, &interval);
}

BOOLEAN
WaypointTimingShouldSkipCycle(
    _In_ ULONG SkipPercentage
)
{
    if (SkipPercentage == 0) {
        return FALSE;
    }
    
    if (SkipPercentage >= 100) {
        return TRUE;
    }

    return (WaypointTimingRandom(0, 99) < SkipPercentage);
}

SHORT
WaypointTimingJitter(
    _In_ SHORT Value,
    _In_ ULONG Amplitude
)
{
    ULONG jitterOffset;
    SHORT result;

    if (Value == 0 || Amplitude == 0) {
        return Value;
    }

    jitterOffset = WaypointTimingRandom(0, Amplitude * 2);
    result = Value + (SHORT)jitterOffset - (SHORT)Amplitude;

    /* Clamp to valid HID limits */
    if (result < WAYPOINT_DELTA_MIN) {
        result = WAYPOINT_DELTA_MIN;
    } else if (result > WAYPOINT_DELTA_MAX) {
        result = WAYPOINT_DELTA_MAX;
    }

    return result;
}
