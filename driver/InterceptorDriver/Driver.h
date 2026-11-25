#pragma once
#define POOL_ZERO_DOWN_LEVEL_SUPPORT
#include <ntifs.h>
#include <wdf.h>
#include <ntddkbd.h>
#include <ntddmou.h>
#include <kbdmou.h>
#include <hidclass.h>
#include <hidpddi.h>
#include "../Shared/InterceptorProtocol.h"
#if !IC_PRODUCT_HAS_CONTROLLERS && (defined(INTERCEPTOR_EXPERIMENTAL_XUSB) || defined(INTERCEPTOR_EXPERIMENTAL_XINPUTHID) || defined(INTERCEPTOR_EXPERIMENTAL_GIP))
#error Controller transports cannot be enabled in the keyboard/mouse product.
#endif
#if IC_PRODUCT_HAS_CONTROLLERS && (!defined(INTERCEPTOR_EXPERIMENTAL_XUSB) || !defined(INTERCEPTOR_EXPERIMENTAL_XINPUTHID) || !defined(INTERCEPTOR_EXPERIMENTAL_GIP))
#error The controller product requires all of its advertised transport backends.
#endif
#include "../Shared/Ring.h"
#include "../Shared/InputRead.h"
#include "../Shared/DeviceQuery.h"
#include "../Shared/CaptureAll.h"
#define FILTER_TAG 'pIcI'
typedef struct MAPPER_FILE_CONTEXT {
    LIST_ENTRY link;
    PFILE_OBJECT file;
} MAPPER_FILE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(MAPPER_FILE_CONTEXT,MapperFile)

BOOLEAN IcAllCaptureLocked(ULONG type);
BOOLEAN IcAllAttachLocked(ULONG type);
void IcAllReadersReadyLocked(void);
typedef struct DEVICE_CONTEXT {
    WDFDEVICE device;
    WDFWAITLOCK stateLock;
    PHIDP_PREPARSED_DATA preparsed;
    HIDP_CAPS caps;
    PUCHAR report;
    WDFREQUEST readRequest;
    WDFMEMORY readMemory;
    KEVENT readDone;
    KEVENT configurationEvent;
    KEVENT readerStarted;
    KEVENT stopEvent;
    KEVENT maintenanceEvent;
    NTSTATUS readStatus;
    ULONG_PTR readBytes;
    BOOLEAN readUsed;
    PFILE_OBJECT hidFile;
    PHIDP_VALUE_CAPS values;
    USHORT valueCount;
    PUSAGE buttonUsages;
    ULONG buttonUsageCapacity;
    MOUSE_ATTRIBUTES mouseAttributes;
    KEYBOARD_ATTRIBUTES keyboardAttributes;
    USHORT indicatorMask;
    BOOLEAN flipWheel,flipHorizontal,forceAbsolute;
    // Descriptor index + 1, by report ID and X/Y/wheel/horizontal wheel.
    // Zero means absent. Built once in PrepareHardware, immutable while reading.
    USHORT axisCaps[256][4];
    PUCHAR keyReports;
    USHORT keyCounts[256];
    UCHAR buttonReports[256];
    USHORT buttonOwners[8];
    UCHAR physicalButtons;
    UCHAR mouseModes[256]; // Coordinate presence/mode for each HID report ID.
    LONG absoluteX,absoluteY;
    USAGE repeatUsage;
    ULONGLONG repeatAt;
    volatile LONG indicators;
    KEYBOARD_TYPEMATIC_PARAMETERS typematic;
    PETHREAD thread;
    volatile LONG stop;
    CONNECT_DATA connection;
    ULONG id;
    ULONG captureId;
    IC_DEVICE_INFO info;
    USHORT instance[256];
    BOOLEAN keyboard,registered;
    UCHAR keys[256],mapped[256],buttons;
    HIDP_KEYBOARD_MODIFIER_STATE modifiers;
    USHORT prefix;
    LIST_ENTRY link;
    BOOLEAN physicalCtrl,physicalAlt,physicalDelete;
    volatile LONG recover;
    UCHAR deliveredKeys[384];
    UCHAR deliveredButtons;
    BOOLEAN powered,readerReady;
    LIST_ENTRY readFiles;
    WDFTIMER retryTimer;
    volatile LONG indicatorDirty;
} DEVICE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT,FilterDevice)
static __inline BOOLEAN IcKeyboardMouseConnected(const DEVICE_CONTEXT* d) {
    return (BOOLEAN)(d->connection.ClassService!=NULL);
}
static __inline BOOLEAN IcKeyboardMouseReady(const DEVICE_CONTEXT* d) {
    return (BOOLEAN)(d->readerReady && !d->stop && IcKeyboardMouseConnected(d));
}
extern WDFSPINLOCK gLock;
extern WDFWAITLOCK gGate;
void IcTrialResumeLocked(void); // Power/start transitions only; caller holds gGate.
NTSTATUS IcRegister(DEVICE_CONTEXT* d);
void IcUnregister(DEVICE_CONTEXT* d);
void IcPhysicalKeys(DEVICE_CONTEXT* d,const UCHAR current[256]);
void IcRoute(DEVICE_CONTEXT* d,PVOID first);
BOOLEAN IcRecovery(DEVICE_CONTEXT* d);
void IcDeviceStopping(DEVICE_CONTEXT* d);
void IcDeviceReadyLocked(DEVICE_CONTEXT* d); // Caller holds gGate while publishing thread.
NTSTATUS FilterPrepare(WDFDEVICE,WDFCMRESLIST,WDFCMRESLIST);
NTSTATUS FilterRelease(WDFDEVICE,WDFCMRESLIST);
NTSTATUS FilterStart(WDFDEVICE);
void FilterStop(WDFDEVICE);
NTSTATUS FilterSuspend(WDFDEVICE);
EVT_WDFDEVICE_WDM_IRP_PREPROCESS FilterDeviceControl;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL FilterInternal;
EVT_WDF_IO_QUEUE_IO_DEFAULT FilterMapperCreate;
EVT_WDF_DEVICE_FILE_CREATE FilterHidCreate;
EVT_WDF_FILE_CLEANUP FilterHidCleanup;
EVT_WDF_DRIVER_DEVICE_ADD FilterAdd;
NTSTATUS IcControlInit(WDFDRIVER driver);
void IcControlDelete(void);
NTSTATUS IcFdoAdded(void);
void IcFdoRemoved(void);
void IcCacheDeviceInstance(WDFDEVICE device,USHORT instance[256]);
NTSTATUS FilterInitialize(WDFDEVICE device,BOOLEAN keyboard);
