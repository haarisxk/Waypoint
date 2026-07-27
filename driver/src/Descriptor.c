// Descriptor.c
// 
// This holds the actual HID Report Descriptor that tells Windows exactly what
// kind of device we are (a 5-button mouse with a scroll wheel). It also handles
// packing the raw data into that format.

#include "Waypoint.h"

// The master blueprint for our virtual mouse. We define 5 buttons, a 16-bit X/Y axis, and an 8-bit scroll wheel.
static const UCHAR g_WaypointReportDescriptor[] = {
    0x05, 0x01,         // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,         // USAGE (Mouse)
    0xA1, 0x01,         // COLLECTION (Application)
    0x09, 0x01,         //   USAGE (Pointer)
    0xA1, 0x00,         //   COLLECTION (Physical)
                        //     Buttons (5 bits)
    0x05, 0x09,         //     USAGE_PAGE (Button)
    0x19, 0x01,         //     USAGE_MINIMUM (Button 1)
    0x29, 0x05,         //     USAGE_MAXIMUM (Button 5)
    0x15, 0x00,         //     LOGICAL_MINIMUM (0)
    0x25, 0x01,         //     LOGICAL_MAXIMUM (1)
    0x95, 0x05,         //     REPORT_COUNT (5)
    0x75, 0x01,         //     REPORT_SIZE (1)
    0x81, 0x02,         //     INPUT (Data,Var,Abs)
                        //     Padding (3 bits)
    0x95, 0x01,         //     REPORT_COUNT (1)
    0x75, 0x03,         //     REPORT_SIZE (3)
    0x81, 0x03,         //     INPUT (Cnst,Var,Abs)
                        //     X and Y (16-bit each)
    0x05, 0x01,         //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,         //     USAGE (X)
    0x09, 0x31,         //     USAGE (Y)
    0x16, 0x00, 0x80,   //     LOGICAL_MINIMUM (-32768)
    0x26, 0xFF, 0x7F,   //     LOGICAL_MAXIMUM (32767)
    0x75, 0x10,         //     REPORT_SIZE (16)
    0x95, 0x02,         //     REPORT_COUNT (2)
    0x81, 0x06,         //     INPUT (Data,Var,Rel)
                        //     Wheel (8-bit)
    0x09, 0x38,         //     USAGE (Wheel)
    0x15, 0x81,         //     LOGICAL_MINIMUM (-127)
    0x25, 0x7F,         //     LOGICAL_MAXIMUM (127)
    0x75, 0x08,         //     REPORT_SIZE (8)
    0x95, 0x01,         //     REPORT_COUNT (1)
    0x81, 0x06,         //     INPUT (Data,Var,Rel)
    0xC0,               //   END_COLLECTION
    0xC0                // END_COLLECTION
};

_Must_inspect_result_
PUCHAR
WaypointGetReportDescriptor(
    _Out_ PUSHORT DescriptorLength
)
{
    *DescriptorLength = sizeof(g_WaypointReportDescriptor);
    return (PUCHAR)g_WaypointReportDescriptor;
}

VOID
WaypointPackMouseReport(
    _Out_writes_bytes_(WAYPOINT_HID_REPORT_SIZE) PUCHAR ReportBuffer,
    _In_ UCHAR  Buttons,
    _In_ SHORT  DeltaX,
    _In_ SHORT  DeltaY,
    _In_ CHAR   Wheel
)
{
    // The buffer is exactly 6 bytes. We don't include a report ID byte here because VHF handles that automatically.
    
    // Keep the wheel values within a safe standard range
    if (Wheel < WAYPOINT_WHEEL_MIN) {
        Wheel = WAYPOINT_WHEEL_MIN;
    } else if (Wheel > WAYPOINT_WHEEL_MAX) {
        Wheel = WAYPOINT_WHEEL_MAX;
    }

    // Pack it up! (1 byte Buttons, 2 bytes X, 2 bytes Y, 1 byte Wheel)
    ReportBuffer[0] = Buttons & WAYPOINT_BUTTON_MASK;
    ReportBuffer[1] = (UCHAR)(DeltaX & 0xFF);
    ReportBuffer[2] = (UCHAR)((DeltaX >> 8) & 0xFF);
    ReportBuffer[3] = (UCHAR)(DeltaY & 0xFF);
    ReportBuffer[4] = (UCHAR)((DeltaY >> 8) & 0xFF);
    ReportBuffer[5] = (UCHAR)Wheel;
}
