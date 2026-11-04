// The keyboard/mouse product links no controller transport or report FIFO.
// Common control paths use these zero-state stubs; public controller IOCTLs
// are rejected by the product policy before dispatch.
#include "ControllerHid.h"
#include "ControllerXusb.h"
#include "ControllerXinputHid.h"
void IcHidInitialize(void) {}
void IcHidRefreshCaptureLocked(void) {}
void IcHidResetLocked(void) {}
ULONG IcHidCountLocked(void) {return 0;}
ULONG IcHidSnapshotLocked(IC_DEVICE_INFO* output,ULONG capacity) {UNREFERENCED_PARAMETER(output);UNREFERENCED_PARAMETER(capacity);return 0;}
NTSTATUS IcHidArmLocked(PKEVENT event) {UNREFERENCED_PARAMETER(event);return STATUS_NOT_SUPPORTED;}
NTSTATUS IcHidCaptureLocked(PKEVENT event,IC_DEVICE_INFO* devices,ULONG capacity,ULONG* count) {UNREFERENCED_PARAMETER(event);UNREFERENCED_PARAMETER(devices);UNREFERENCED_PARAMETER(capacity);*count=0;return STATUS_NOT_SUPPORTED;}
NTSTATUS IcHidGetLocked(ULONG requested,PVOID buffer,SIZE_T length,SIZE_T* written) {UNREFERENCED_PARAMETER(requested);UNREFERENCED_PARAMETER(buffer);UNREFERENCED_PARAMETER(length);*written=0;return STATUS_NOT_SUPPORTED;}
NTSTATUS IcHidGetSelectedLocked(const IC_GET_SELECT* selected,PVOID buffer,SIZE_T length,SIZE_T* written) {UNREFERENCED_PARAMETER(selected);UNREFERENCED_PARAMETER(buffer);UNREFERENCED_PARAMETER(length);*written=0;return STATUS_NOT_SUPPORTED;}
NTSTATUS IcHidSendLocked(PVOID buffer,SIZE_T length,IC_SEND_RESULT* result,BOOLEAN owned) {UNREFERENCED_PARAMETER(buffer);UNREFERENCED_PARAMETER(length);UNREFERENCED_PARAMETER(result);UNREFERENCED_PARAMETER(owned);return STATUS_NOT_SUPPORTED;}
NTSTATUS IcHidQueryLocked(ULONG token,IC_DEVICE_QUERY_RESULT* result,BOOLEAN ordinal) {UNREFERENCED_PARAMETER(token);UNREFERENCED_PARAMETER(result);UNREFERENCED_PARAMETER(ordinal);return STATUS_NOT_SUPPORTED;}
NTSTATUS IcHidDescriptorLocked(ULONG id,PVOID buffer,SIZE_T length,SIZE_T* written) {UNREFERENCED_PARAMETER(id);UNREFERENCED_PARAMETER(buffer);UNREFERENCED_PARAMETER(length);*written=0;return STATUS_NOT_SUPPORTED;}
ULONG IcHidQueuedLocked(void) {return 0;}
void IcXusbInitialize(void) {}
void IcXusbRefreshCaptureLocked(void) {}
void IcXusbResetLocked(void) {}
ULONG IcXusbCountLocked(void) {return 0;}
ULONG IcXusbSnapshotLocked(IC_DEVICE_INFO* output,ULONG capacity) {UNREFERENCED_PARAMETER(output);UNREFERENCED_PARAMETER(capacity);return 0;}
NTSTATUS IcXusbArmLocked(PKEVENT event) {UNREFERENCED_PARAMETER(event);return STATUS_NOT_SUPPORTED;}
NTSTATUS IcXusbCaptureLocked(PKEVENT event,IC_DEVICE_INFO* devices,ULONG capacity,ULONG* count) {UNREFERENCED_PARAMETER(event);UNREFERENCED_PARAMETER(devices);UNREFERENCED_PARAMETER(capacity);*count=0;return STATUS_NOT_SUPPORTED;}
NTSTATUS IcXusbGetLocked(ULONG requested,PVOID buffer,SIZE_T length,SIZE_T* written) {UNREFERENCED_PARAMETER(requested);UNREFERENCED_PARAMETER(buffer);UNREFERENCED_PARAMETER(length);*written=0;return STATUS_NOT_SUPPORTED;}
NTSTATUS IcXusbGetSelectedLocked(const IC_GET_SELECT* selected,PVOID buffer,SIZE_T length,SIZE_T* written) {UNREFERENCED_PARAMETER(selected);UNREFERENCED_PARAMETER(buffer);UNREFERENCED_PARAMETER(length);*written=0;return STATUS_NOT_SUPPORTED;}
NTSTATUS IcXusbSendLocked(PVOID buffer,SIZE_T length,IC_SEND_RESULT* result,BOOLEAN owned) {UNREFERENCED_PARAMETER(buffer);UNREFERENCED_PARAMETER(length);UNREFERENCED_PARAMETER(result);UNREFERENCED_PARAMETER(owned);return STATUS_NOT_SUPPORTED;}
NTSTATUS IcXusbQueryLocked(ULONG token,IC_DEVICE_QUERY_RESULT* result,BOOLEAN ordinal) {UNREFERENCED_PARAMETER(token);UNREFERENCED_PARAMETER(result);UNREFERENCED_PARAMETER(ordinal);return STATUS_NOT_SUPPORTED;}
void IcXinputHidInitialize(void) {}
void IcXinputHidRefreshCaptureLocked(void) {}
void IcXinputHidResetLocked(void) {}
ULONG IcXinputHidCountLocked(void) {return 0;}
ULONG IcXinputHidSnapshotLocked(IC_DEVICE_INFO* output,ULONG capacity) {UNREFERENCED_PARAMETER(output);UNREFERENCED_PARAMETER(capacity);return 0;}
NTSTATUS IcXinputHidArmLocked(PKEVENT event) {UNREFERENCED_PARAMETER(event);return STATUS_NOT_SUPPORTED;}
NTSTATUS IcXinputHidCaptureLocked(PKEVENT event,IC_DEVICE_INFO* devices,ULONG capacity,ULONG* count) {UNREFERENCED_PARAMETER(event);UNREFERENCED_PARAMETER(devices);UNREFERENCED_PARAMETER(capacity);*count=0;return STATUS_NOT_SUPPORTED;}
NTSTATUS IcXinputHidGetLocked(ULONG requested,PVOID buffer,SIZE_T length,SIZE_T* written) {UNREFERENCED_PARAMETER(requested);UNREFERENCED_PARAMETER(buffer);UNREFERENCED_PARAMETER(length);*written=0;return STATUS_NOT_SUPPORTED;}
NTSTATUS IcXinputHidGetSelectedLocked(const IC_GET_SELECT* selected,PVOID buffer,SIZE_T length,SIZE_T* written) {UNREFERENCED_PARAMETER(selected);UNREFERENCED_PARAMETER(buffer);UNREFERENCED_PARAMETER(length);*written=0;return STATUS_NOT_SUPPORTED;}
NTSTATUS IcXinputHidSendLocked(PVOID buffer,SIZE_T length,IC_SEND_RESULT* result,BOOLEAN owned) {UNREFERENCED_PARAMETER(buffer);UNREFERENCED_PARAMETER(length);UNREFERENCED_PARAMETER(result);UNREFERENCED_PARAMETER(owned);return STATUS_NOT_SUPPORTED;}
NTSTATUS IcXinputHidQueryLocked(ULONG token,IC_DEVICE_QUERY_RESULT* result,BOOLEAN ordinal) {UNREFERENCED_PARAMETER(token);UNREFERENCED_PARAMETER(result);UNREFERENCED_PARAMETER(ordinal);return STATUS_NOT_SUPPORTED;}
NTSTATUS IcXinputHidDescriptorLocked(ULONG id,PVOID buffer,SIZE_T length,SIZE_T* written) {UNREFERENCED_PARAMETER(id);UNREFERENCED_PARAMETER(buffer);UNREFERENCED_PARAMETER(length);*written=0;return STATUS_NOT_SUPPORTED;}
ULONG IcXinputHidQueuedLocked(void) {return 0;}

