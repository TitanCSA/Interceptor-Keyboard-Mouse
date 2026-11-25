#include "Driver.h"
#include "HidInputBuffers.h"
#include "../Shared/HidMapperProfile.h"
#include "../Shared/AxisCache.h"
#include "../Shared/HidKeyState.h"
#include "../Shared/MouseReportMode.h"
// HID mapper adapted from the local ConfigCSASoftwareWebV2 project.
static SHORT FilterScaleWheel(LONG value,BOOLEAN reverse) {
    const LONGLONG units=(LONGLONG)value*(reverse?-120:120);
    return (SHORT)max(-32768,min(32767,units));
}




static void FilterStartLocked(WDFDEVICE device);
static void FilterReaderReady(DEVICE_CONTEXT* d);
static void FilterApplyIndicators(DEVICE_CONTEXT* d);
static void FilterStopLocked(WDFDEVICE device);
static NTSTATUS FilterIndicators(DEVICE_CONTEXT* d,USHORT flags);
static void FilterRepeat(DEVICE_CONTEXT* d);
// CreateLocked counts and publishes its snapshot under gGate. Every change of
// reader eligibility must use that same gate, including terminal/error paths.
// Never hold it while creating, cancelling or joining the reader thread.
static void FilterSetStop(DEVICE_CONTEXT* d,LONG stopped) {
    WdfWaitLockAcquire(gGate,NULL);
    InterlockedExchange(&d->stop,stopped);
    WdfWaitLockRelease(gGate);
}
_Ret_writes_maybenull_(count)
static PHIDP_VALUE_CAPS FilterAllocateValues(ULONG count) {
    if(!count || count>512)return NULL;
    return ExAllocatePoolZero(NonPagedPoolNx,(SIZE_T)count*sizeof(HIDP_VALUE_CAPS),FILTER_TAG);
}
_Ret_writes_maybenull_(count)
static PHIDP_BUTTON_CAPS FilterAllocateButtons(ULONG count) {
    if(!count || count>512)return NULL;
    return ExAllocatePoolZero(NonPagedPoolNx,(SIZE_T)count*sizeof(HIDP_BUTTON_CAPS),FILTER_TAG);
}
// Keep allocation capacity separate from the HID parser's in/out count.
// Explicit array contracts also preserve the readable extent across HIDP's
// aliased count annotations in older MSVC analysis engines.
static NTSTATUS FilterValueCaps(PHIDP_PREPARSED_DATA data,ULONG capacity,
    _Out_writes_to_(capacity,*count) PHIDP_VALUE_CAPS values,_Out_ PUSHORT count) {
    NTSTATUS status;*count=(USHORT)capacity;
    status=HidP_GetValueCaps(HidP_Input,values,count,data);
    if(status!=HIDP_STATUS_SUCCESS || *count>capacity) { *count=0;return status==HIDP_STATUS_SUCCESS?STATUS_INVALID_BUFFER_SIZE:status; }
    return status;
}
static NTSTATUS FilterButtonCaps(PHIDP_PREPARSED_DATA data,HIDP_REPORT_TYPE type,ULONG capacity,
    _Out_writes_to_(capacity,*count) PHIDP_BUTTON_CAPS caps,_Out_ PUSHORT count) {
    NTSTATUS status;*count=(USHORT)capacity;
    status=HidP_GetButtonCaps(type,caps,count,data);
    if(status!=HIDP_STATUS_SUCCESS || *count>capacity) { *count=0;return status==HIDP_STATUS_SUCCESS?STATUS_INVALID_BUFFER_SIZE:status; }
    return status;
}
static void FilterReadComplete(WDFREQUEST request,WDFIOTARGET target,PWDF_REQUEST_COMPLETION_PARAMS parameters,WDFCONTEXT context) {
    DEVICE_CONTEXT* d=context;UNREFERENCED_PARAMETER(request);UNREFERENCED_PARAMETER(target);

    d->readStatus=parameters->IoStatus.Status;d->readBytes=parameters->IoStatus.Information;
    KeSetEvent(&d->readDone,IO_NO_INCREMENT,FALSE);
}
static NTSTATUS FilterRead(DEVICE_CONTEXT* d,ULONG_PTR* bytes) {
    NTSTATUS status;WDF_REQUEST_REUSE_PARAMS reuse;ULONG recoveryDelay=1;
    WDFIOTARGET target=WdfDeviceGetIoTarget(d->device);
    if(d->readUsed) {
        WDF_REQUEST_REUSE_PARAMS_INIT(&reuse,WDF_REQUEST_REUSE_NO_FLAGS,STATUS_SUCCESS);
        status=WdfRequestReuse(d->readRequest,&reuse);if(!NT_SUCCESS(status))return status;
    }
    d->readUsed=TRUE;KeClearEvent(&d->readDone);
    status=WdfIoTargetFormatRequestForRead(target,d->readRequest,d->readMemory,NULL,NULL);if(!NT_SUCCESS(status))return status;
    IoGetNextIrpStackLocation(WdfRequestWdmGetIrp(d->readRequest))->FileObject=d->hidFile;
    WdfRequestSetCompletionRoutine(d->readRequest,FilterReadComplete,d);
    if(!WdfRequestSend(d->readRequest,target,NULL))return WdfRequestGetStatus(d->readRequest);
    if(!KeReadStateEvent(&d->readDone) || (NT_SUCCESS(d->readStatus) && d->readBytes==d->caps.InputReportByteLength))
        FilterReaderReady(d);
    for(;;) {
        PVOID events[3]={&d->stopEvent,&d->readDone,&d->maintenanceEvent};
        LARGE_INTEGER timeout={0};PLARGE_INTEGER due=NULL;
        // No polling or periodic cancellation at idle. Only held-key repeat or
        // a full class queue needs a deadline; neither cancels the pending read.
        if(!IcRecovery(d)) {
            timeout.QuadPart=-(LONGLONG)recoveryDelay*10000;due=&timeout;
            recoveryDelay=min(recoveryDelay*2,16u);
        } else recoveryDelay=1;
        if(d->keyboard && d->repeatUsage) {
            ULONGLONG now=KeQueryInterruptTime();
            LONGLONG ticks=d->repeatAt>now?(LONGLONG)(d->repeatAt-now):1;
            if(!due || ticks < -timeout.QuadPart) { timeout.QuadPart=-ticks;due=&timeout; }
        }
        status=KeWaitForMultipleObjects(3,events,WaitAny,Executive,KernelMode,FALSE,due,NULL);
        if(status==STATUS_WAIT_0) {
            // Request storage and hidFile remain alive until completion, even
            // if cancellation races an already completed read.
            WdfRequestCancelSentRequest(d->readRequest);
            KeWaitForSingleObject(&d->readDone,Executive,KernelMode,FALSE,NULL);
            return STATUS_CANCELLED;
        }
        if(status==STATUS_WAIT_1)break;
        if(d->keyboard && !d->stop) {FilterApplyIndicators(d);FilterRepeat(d);}
    }
    *bytes=d->readBytes;return d->readStatus;
}

static void FilterPickReadFile(DEVICE_CONTEXT* d) {
    WdfWaitLockAcquire(gGate,NULL);
    d->hidFile=IsListEmpty(&d->readFiles)?NULL:
        CONTAINING_RECORD(d->readFiles.Flink,MAPPER_FILE_CONTEXT,link)->file;
    WdfWaitLockRelease(gGate);
}
static NTSTATUS FilterTransfer(DEVICE_CONTEXT* d,BOOLEAN write,PVOID buffer,ULONG length,LONGLONG timeout,ULONG_PTR* bytes) {
    WDFREQUEST request;WDFMEMORY memory;WDF_OBJECT_ATTRIBUTES attributes;WDF_REQUEST_SEND_OPTIONS options;
    WDFIOTARGET target=WdfDeviceGetIoTarget(d->device);NTSTATUS status;
    status=WdfRequestCreate(WDF_NO_OBJECT_ATTRIBUTES,target,&request);if(!NT_SUCCESS(status))return status;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ParentObject=request;
    status=WdfMemoryCreatePreallocated(&attributes,buffer,length,&memory);
    if(NT_SUCCESS(status))status=write?WdfIoTargetFormatRequestForWrite(target,request,memory,NULL,NULL):WdfIoTargetFormatRequestForRead(target,request,memory,NULL,NULL);
    if(NT_SUCCESS(status)) {
        IoGetNextIrpStackLocation(WdfRequestWdmGetIrp(request))->FileObject=d->hidFile;
        WDF_REQUEST_SEND_OPTIONS_INIT(&options,WDF_REQUEST_SEND_OPTION_SYNCHRONOUS|WDF_REQUEST_SEND_OPTION_TIMEOUT);
        WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options,timeout);
        WdfRequestSend(request,target,&options);status=WdfRequestGetStatus(request);
        if(bytes)*bytes=WdfRequestGetInformation(request);
    }
    WdfObjectDelete(request);return status;
}
void FilterMapperCreate(WDFQUEUE queue,WDFREQUEST request) {
    FilterHidCreate(WdfIoQueueGetDevice(queue),request,WdfRequestGetFileObject(request));
}
void FilterHidCreate(WDFDEVICE device,WDFREQUEST request,WDFFILEOBJECT file) {
    DEVICE_CONTEXT* d=FilterDevice(device);
    WDF_REQUEST_SEND_OPTIONS options;
    NTSTATUS status;
    PFILE_OBJECT wdm=WdfFileObjectWdmGetFileObject(file);
    PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(WdfRequestWdmGetIrp(request));
    const ACCESS_MASK access=stack->Parameters.Create.SecurityContext?stack->Parameters.Create.SecurityContext->DesiredAccess:0;
    const BOOLEAN kernel=WdfRequestGetRequestorMode(request)==KernelMode;

    // The lower HID driver owns the file context. Forward the original CREATE
    // and preserve its completion even when the input worker needs to retry.
    WdfRequestFormatRequestUsingCurrentType(request);
    WDF_REQUEST_SEND_OPTIONS_INIT(&options,WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
    WdfRequestSend(request,WdfDeviceGetIoTarget(device),&options);
    status=WdfRequestGetStatus(request);
    if(NT_SUCCESS(status) && kernel && wdm && (wdm->ReadAccess || (access&(FILE_READ_DATA|GENERIC_READ)))) {
        MAPPER_FILE_CONTEXT* entry=MapperFile(file);
        WdfWaitLockAcquire(d->stateLock,NULL);
        entry->file=wdm;
        InsertTailList(&d->readFiles,&entry->link);
        if(!d->hidFile)FilterPickReadFile(d);
        FilterStartLocked(device);
        WdfWaitLockRelease(d->stateLock);
    }
    WdfRequestCompleteWithInformation(request,status,WdfRequestGetInformation(request));
}

void FilterHidCleanup(WDFFILEOBJECT file) {
    DEVICE_CONTEXT* d=FilterDevice(WdfFileObjectGetDevice(file));
    MAPPER_FILE_CONTEXT* entry=MapperFile(file);
    WdfWaitLockAcquire(d->stateLock,NULL);
    if(entry->file) {
        const BOOLEAN inUse=entry->file==d->hidFile;
        if(inUse)FilterStopLocked(d->device);
        RemoveEntryList(&entry->link);
        entry->file=NULL;
        if(inUse) {
            FilterPickReadFile(d);
            FilterStartLocked(d->device);
        }
    }
    WdfWaitLockRelease(d->stateLock);
}

static NTSTATUS FilterHidIoctl(WDFDEVICE device, ULONG code, PVOID output, ULONG length) {
    WDF_MEMORY_DESCRIPTOR memory; WDF_REQUEST_SEND_OPTIONS options;
    NTSTATUS status;
    ULONG attempt=0;
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memory, output, length);
    WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_TIMEOUT);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options, WDF_REL_TIMEOUT_IN_SEC(2));
    for(;;) {
        LARGE_INTEGER retry;
        RtlZeroMemory(output,length);
        status=WdfIoTargetSendIoctlSynchronously(WdfDeviceGetIoTarget(device),NULL,code,
            NULL,&memory,&options,NULL);
        // The remaining legacy mapper can see a transient not-ready response
        // while the installed devices are being migrated to the native stack.
        if((status!=STATUS_DEVICE_NOT_READY && status!=STATUS_DEVICE_NOT_CONNECTED) || attempt==2)return status;
        retry.QuadPart=-(LONGLONG)(25u<<attempt)*10000;
        ++attempt;
        (void)KeDelayExecutionThread(KernelMode,FALSE,&retry);
    }
}
static void FilterFailure(NTSTATUS status) { UNREFERENCED_PARAMETER(status); }
static NTSTATUS FilterPrepareFailure(ULONG code,NTSTATUS status) {
    if(NT_SUCCESS(status))status=STATUS_DEVICE_CONFIGURATION_ERROR;
    DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,
        "Interceptor HID reader initialization failed: operation=%08x status=%08x\n",code,(ULONG)status);
    return status;
}
static void FilterCacheMouseModes(DEVICE_CONTEXT* d) {
    ULONG report;
    for(report=0;report<RTL_NUMBER_OF(d->mouseModes);++report) {
        USHORT x=d->axisCaps[report][0],y=d->axisCaps[report][1];
        d->mouseModes[report]=IcMouseReportMode(x!=0,x?d->values[x-1].IsAbsolute:FALSE,
            y!=0,y?d->values[y-1].IsAbsolute:FALSE);
    }
}
static ULONG FilterReadSetting(WDFKEY key,PCWSTR name) {
    UNICODE_STRING value;
    ULONG number=0;
    RtlInitUnicodeString(&value,name);
    if(!NT_SUCCESS(WdfRegistryQueryULong(key,&value,&number)))return 0;
    return number;
}

static void FilterReadMouseSettings(DEVICE_CONTEXT* d) {
    WDFKEY key;
    d->flipWheel=d->flipHorizontal=d->forceAbsolute=FALSE;
    if(!NT_SUCCESS(WdfDeviceOpenRegistryKey(d->device,PLUGPLAY_REGKEY_DEVICE,
        KEY_QUERY_VALUE,WDF_NO_OBJECT_ATTRIBUTES,&key)))return;
    d->flipWheel=FilterReadSetting(key,L"FlipFlopWheel")!=0;
    d->flipHorizontal=FilterReadSetting(key,L"FlipFlopHScroll")!=0;
    d->forceAbsolute=FilterReadSetting(key,L"ForceAbsolute")!=0;
    WdfRegistryClose(key);
}

static NTSTATUS FilterDeclaredUsages(DEVICE_CONTEXT* d,HIDP_REPORT_TYPE type,USHORT page,UCHAR usages[256]) {
    const USHORT capacity=type==HidP_Input?d->caps.NumberInputButtonCaps:d->caps.NumberOutputButtonCaps;
    USHORT count=capacity;
    PHIDP_BUTTON_CAPS buttons;
    ULONG i,usage;
    if(!capacity)return STATUS_SUCCESS;
    buttons=FilterAllocateButtons(capacity);
    if(!buttons)return STATUS_INSUFFICIENT_RESOURCES;
    {
        const NTSTATUS status=FilterButtonCaps(d->preparsed,type,capacity,buttons,&count);
        if(status!=HIDP_STATUS_SUCCESS) {
            ExFreePoolWithTag(buttons,FILTER_TAG);
            return status;
        }
    }
    for(i=0;i<count;++i) {
        const HIDP_BUTTON_CAPS* button=&buttons[i];
        const ULONG first=button->IsRange?button->Range.UsageMin:button->NotRange.Usage;
        const ULONG last=button->IsRange?button->Range.UsageMax:button->NotRange.Usage;
        if(button->UsagePage==page)for(usage=first;usage<=last && usage<256;++usage)usages[usage]=TRUE;
    }
    ExFreePoolWithTag(buttons,FILTER_TAG);
    return STATUS_SUCCESS;
}

static void FilterDescribeInput(DEVICE_CONTEXT* d) {
    if(d->keyboard) {
        KEYBOARD_ATTRIBUTES attributes;
        USHORT indicators,keys=0,functions=0;
        ULONG i;
        UCHAR usages[256]={0};
        WdfWaitLockAcquire(gGate,NULL);
        attributes=d->keyboardAttributes;
        indicators=d->indicatorMask;
        WdfWaitLockRelease(gGate);
        if(FilterDeclaredUsages(d,HidP_Input,7,usages)==STATUS_SUCCESS) {
            for(i=4;i<RTL_NUMBER_OF(usages);++i)if(usages[i])++keys;
            for(i=0x3a;i<=0x45;++i)if(usages[i])++functions;
            for(i=0x68;i<=0x73;++i)if(usages[i])++functions;
            attributes.NumberOfKeysTotal=keys;
            attributes.NumberOfFunctionKeys=functions;
        }
        RtlZeroMemory(usages,sizeof(usages));
        if(FilterDeclaredUsages(d,HidP_Output,8,usages)==STATUS_SUCCESS) {
            attributes.NumberOfIndicators=(USHORT)(usages[1]+usages[2]+usages[3]);
            indicators=(USHORT)((usages[1]?KEYBOARD_NUM_LOCK_ON:0) |
                (usages[2]?KEYBOARD_CAPS_LOCK_ON:0) | (usages[3]?KEYBOARD_SCROLL_LOCK_ON:0));
        }
        WdfWaitLockAcquire(gGate,NULL);
        d->keyboardAttributes=attributes;
        d->indicatorMask=indicators;
        WdfWaitLockRelease(gGate);
    } else {
        MOUSE_ATTRIBUTES attributes={0};
        ULONG report;
        BOOLEAN vertical=FALSE,horizontal=FALSE;
        const ULONG maxUsages=HidP_MaxUsageListLength(HidP_Input,9,d->preparsed);
        d->buttonUsageCapacity=min(maxUsages,1024u);
        if(d->buttonUsageCapacity>32)
            d->buttonUsages=ExAllocatePoolZero(NonPagedPoolNx,d->buttonUsageCapacity*sizeof(USAGE),FILTER_TAG);
        for(report=0;report<RTL_NUMBER_OF(d->axisCaps);++report) {
            if(d->axisCaps[report][2])vertical=TRUE;
            if(d->axisCaps[report][3])horizontal=TRUE;
        }
        attributes.MouseIdentifier=(USHORT)((vertical?WHEELMOUSE_HID_HARDWARE:MOUSE_HID_HARDWARE) |
            (horizontal?HORIZONTAL_WHEEL_PRESENT:0));
        attributes.NumberOfButtons=(USHORT)min(maxUsages,5u);
        attributes.InputDataQueueLength=100*sizeof(MOUSE_INPUT_DATA);
        FilterReadMouseSettings(d);
        WdfWaitLockAcquire(gGate,NULL);
        d->mouseAttributes=attributes;
        WdfWaitLockRelease(gGate);
    }
}

static NTSTATUS FilterPrepareReader(WDFDEVICE device) {
    DEVICE_CONTEXT* d=FilterDevice(device);
    HID_COLLECTION_INFORMATION info;
    IC_DEVICE_INFO profile={0};
    NTSTATUS status;
    ULONG stage=IOCTL_HID_GET_COLLECTION_INFORMATION;
    d->absoluteX=d->absoluteY=0;d->prefix=0;d->modifiers.ul=0;
    RtlZeroMemory(d->mouseModes,sizeof(d->mouseModes));
    RtlZeroMemory(d->axisCaps,sizeof(d->axisCaps));
    RtlZeroMemory(d->keyCounts,sizeof(d->keyCounts));
    RtlZeroMemory(&info, sizeof(info));
    status = FilterHidIoctl(device, stage, &info, sizeof(info));
    if (!NT_SUCCESS(status)) return FilterPrepareFailure(stage,status);
    if (!info.DescriptorSize || info.DescriptorSize > IC_MAPPER_DESCRIPTOR_MAX)
        return FilterPrepareFailure(stage,STATUS_INVALID_BUFFER_SIZE);
    profile.vid=info.VendorID;profile.pid=info.ProductID;profile.revision=info.VersionNumber;
    profile.flags=IC_INFO_HID_VALID;profile.instance_token=d->id;
    {
        ULONG bytes=0;
        if(NT_SUCCESS(WdfDeviceQueryProperty(device,DevicePropertyHardwareID,sizeof(profile.hardware_id)-sizeof(USHORT),profile.hardware_id,&bytes)))
            profile.flags|=IC_INFO_HARDWARE_ID_VALID;
        else RtlZeroMemory(profile.hardware_id,sizeof(profile.hardware_id));
        status=WdfDeviceQueryProperty(device,DevicePropertyFriendlyName,sizeof(profile.name)-sizeof(USHORT),profile.name,&bytes);
        if(!NT_SUCCESS(status))status=WdfDeviceQueryProperty(device,DevicePropertyDeviceDescription,sizeof(profile.name)-sizeof(USHORT),profile.name,&bytes);
        if(NT_SUCCESS(status))profile.flags|=IC_INFO_NAME_VALID;
        else RtlZeroMemory(profile.name,sizeof(profile.name));
    }
    d->preparsed = ExAllocatePoolZero(NonPagedPoolNx, info.DescriptorSize, FILTER_TAG);
    stage=IOCTL_HID_GET_COLLECTION_DESCRIPTOR;
    if (!d->preparsed) return FilterPrepareFailure(stage,STATUS_INSUFFICIENT_RESOURCES);
    status = FilterHidIoctl(device, stage, d->preparsed, info.DescriptorSize);
    if (!NT_SUCCESS(status)) goto fail;
    stage=0;
    status = HidP_GetCaps(d->preparsed, &d->caps);
    if (status != HIDP_STATUS_SUCCESS) goto fail;
    if (IcMapperProfile(info.DescriptorSize,d->caps.UsagePage,d->caps.Usage,
        d->caps.InputReportByteLength,d->caps.NumberInputValueCaps,d->keyboard?6:2)!=IC_MAPPER_PROFILE_OK) {
        status = STATUS_DEVICE_CONFIGURATION_ERROR; goto fail;
    }
    profile.type=d->keyboard?IC_TYPE_KEYBOARD:IC_TYPE_MOUSE;
    profile.usage_page=d->caps.UsagePage;profile.usage=d->caps.Usage;
    profile.input_bytes=d->caps.InputReportByteLength;profile.output_bytes=d->caps.OutputReportByteLength;profile.feature_bytes=d->caps.FeatureReportByteLength;
    d->valueCount=d->caps.NumberInputValueCaps;
    if(d->valueCount) {
        ULONG i;const ULONG capacity=d->valueCount;USHORT count=(USHORT)capacity;
        if(!capacity || capacity>512) { status=STATUS_INVALID_BUFFER_SIZE;goto fail; }
        d->values=FilterAllocateValues(capacity);
        if(!d->values) { status=STATUS_INSUFFICIENT_RESOURCES;goto fail; }
        status=FilterValueCaps(d->preparsed,capacity,d->values,&count);
        if(status!=HIDP_STATUS_SUCCESS)goto fail;
        if(count>capacity) { status=STATUS_INVALID_BUFFER_SIZE;goto fail; }
        d->valueCount=count;
        if(!d->keyboard)for(i=0;i<count;++i) {
            PHIDP_VALUE_CAPS cap=&d->values[i];
            IcCacheAxes(d->axisCaps[cap->ReportID],(USHORT)i,cap->UsagePage,
                cap->IsRange?cap->Range.UsageMin:cap->NotRange.Usage,
                cap->IsRange?cap->Range.UsageMax:cap->NotRange.Usage);
        }
        if(!d->keyboard)FilterCacheMouseModes(d);
    }
    if(d->keyboard) {
        d->keyReports=ExAllocatePoolZero(NonPagedPoolNx,256*32,FILTER_TAG);
        if(!d->keyReports) { status=STATUS_INSUFFICIENT_RESOURCES;goto fail; }
    }
    d->report = ExAllocatePoolZero(NonPagedPoolNx, d->caps.InputReportByteLength, FILTER_TAG);
    if (!d->report) { status = STATUS_INSUFFICIENT_RESOURCES; goto fail; }
    {
        WDF_OBJECT_ATTRIBUTES attributes;WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ParentObject=device;
        status=WdfRequestCreate(&attributes,WdfDeviceGetIoTarget(device),&d->readRequest);if(!NT_SUCCESS(status))goto fail;
        WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ParentObject=d->readRequest;
        status=WdfMemoryCreatePreallocated(&attributes,d->report,d->caps.InputReportByteLength,&d->readMemory);if(!NT_SUCCESS(status))goto fail;
        d->readUsed=FALSE;
    }
    FilterDescribeInput(d);
    WdfWaitLockAcquire(gGate,NULL);
    d->info=profile;
    WdfWaitLockRelease(gGate);
    return STATUS_SUCCESS;
fail:
    if(d->readRequest) { WdfObjectDelete(d->readRequest);d->readRequest=NULL;d->readMemory=NULL; }
    if(d->report) { ExFreePoolWithTag(d->report,FILTER_TAG);d->report=NULL; }
    if(d->values) { ExFreePoolWithTag(d->values,FILTER_TAG);d->values=NULL; }
    if(d->keyReports) { ExFreePoolWithTag(d->keyReports,FILTER_TAG);d->keyReports=NULL; }
    ExFreePoolWithTag(d->preparsed, FILTER_TAG); d->preparsed = NULL;return FilterPrepareFailure(stage,status);
}
static void FilterDeliver(DEVICE_CONTEXT* d,PVOID first,PVOID last) { UNREFERENCED_PARAMETER(last);IcRoute(d,first); }
static BOOLEAN FilterScanCodes(PVOID context, PCHAR codes, ULONG length) {
    DEVICE_CONTEXT* d = context; ULONG i;
    for (i = 0; i < length; ++i) {
        UCHAR code = (UCHAR)codes[i]; KEYBOARD_INPUT_DATA packet = {0};
        if (code == 0xE0 || code == 0xE1) { d->prefix = code == 0xE0 ? KEY_E0 : KEY_E1; continue; }
        packet.MakeCode = code & 0x7F;
        packet.Flags = d->prefix | ((code & 0x80) ? KEY_BREAK : KEY_MAKE); d->prefix = 0;
        FilterDeliver(d, &packet, &packet + 1);
    }
    return TRUE;
}
static void FilterApplyKeys(DEVICE_CONTEXT* d) {
    USAGE changed[256],repeat=0;ULONG n,i;const UCHAR* mapped=d->keys;NTSTATUS status;
    // Only this reader writes keys; delivery callbacks cannot change the union.
    n=IcHidCollectKeyChanges(d->mapped,mapped,FALSE,changed);
    if(n) {
        status=HidP_TranslateUsagesToI8042ScanCodes(changed,n,HidP_Keyboard_Break,&d->modifiers,FilterScanCodes,d);
        if(status!=HIDP_STATUS_SUCCESS)FilterFailure(status);
    }
    if(d->repeatUsage && !mapped[d->repeatUsage])d->repeatUsage=0;
    n=IcHidCollectKeyChanges(d->mapped,mapped,TRUE,changed);
    for(i=0;i<n;++i)if(changed[i]<0xe0)repeat=changed[i];
    if(repeat) {
        USHORT delay;WdfSpinLockAcquire(gLock);delay=d->typematic.Delay;WdfSpinLockRelease(gLock);
        d->repeatUsage=repeat;d->repeatAt=KeQueryInterruptTime()+(ULONGLONG)delay*10000;
    }
    if(n) {
        status=HidP_TranslateUsagesToI8042ScanCodes(changed,n,HidP_Keyboard_Make,&d->modifiers,FilterScanCodes,d);
        if(status!=HIDP_STATUS_SUCCESS)FilterFailure(status);
    }
    RtlCopyMemory(d->mapped,mapped,sizeof(d->mapped));
}
static void FilterRepeat(DEVICE_CONTEXT* d) {
    if(d->repeatUsage && KeQueryInterruptTime()>=d->repeatAt) {
        USHORT rate;NTSTATUS status;
        WdfSpinLockAcquire(gLock);rate=d->typematic.Rate;WdfSpinLockRelease(gLock);
        status=HidP_TranslateUsagesToI8042ScanCodes(&d->repeatUsage,1,HidP_Keyboard_Make,&d->modifiers,FilterScanCodes,d);
        if(status!=HIDP_STATUS_SUCCESS)FilterFailure(status);
        d->repeatAt=KeQueryInterruptTime()+10000000/max(1,rate);
    }
}
static void FilterKeyboard(DEVICE_CONTEXT* d) {
    USAGE usages[256]; ULONG count = RTL_NUMBER_OF(usages), i;
    UCHAR reportKeys[32]={0},current[256];PUCHAR previous=d->keyReports+d->report[0]*32;NTSTATUS status;
    status = HidP_GetUsages(HidP_Input, 7, 0, usages, &count, d->preparsed,
        (PCHAR)d->report, d->caps.InputReportByteLength);
    if(status==HIDP_STATUS_INCOMPATIBLE_REPORT_ID || status==HIDP_STATUS_USAGE_NOT_FOUND) return;
    if (status != HIDP_STATUS_SUCCESS) { FilterFailure(status); return; }
    for (i = 0; i < count; ++i) {
        if (usages[i] >= 1 && usages[i] <= 3) return; // Rollover is not a release-all report.
        if (usages[i] >= 4 && usages[i] < 256) reportKeys[usages[i]/8]|=(UCHAR)(1u<<(usages[i]%8));
    }
    // Most repeats are identical. Compare one compact report before copying or
    // merging the global state; only changed usage bits need owner updates.
    if(RtlCompareMemory(previous,reportKeys,sizeof(reportKeys))==sizeof(reportKeys))return;
    RtlCopyMemory(current,d->keys,sizeof(current));
    if(!IcHidMergeKeyReport(previous,reportKeys,d->keyCounts,current))return;
    IcPhysicalKeys(d,current);
    RtlCopyMemory(d->keys,current,sizeof(current));FilterApplyKeys(d);
}
static LONG FilterAxis(DEVICE_CONTEXT* d,USAGE page,USAGE usage,BOOLEAN* valid) {
    ULONG axis=page==0x0C?3:usage==0x30?0:usage==0x31?1:2;
    USHORT index=d->axisCaps[d->report[0]][axis];
    if(index) {
        PHIDP_VALUE_CAPS cap=&d->values[index-1];ULONG raw=0;LONG result;NTSTATUS status;
        status=HidP_GetUsageValue(HidP_Input,page,cap->LinkCollection,usage,&raw,d->preparsed,(PCHAR)d->report,d->caps.InputReportByteLength);
        if(status!=HIDP_STATUS_SUCCESS) { FilterFailure(status);return 0; }
        if(cap->LogicalMin<0 && cap->BitSize && cap->BitSize<32 && (raw&(1u<<(cap->BitSize-1))))raw|=(~0u<<cap->BitSize);
        result=(LONG)raw;
        if((cap->IsAbsolute || d->forceAbsolute) && page==1 && (usage==0x30 || usage==0x31)) {
            LONGLONG range=(LONGLONG)cap->LogicalMax-cap->LogicalMin;
            if(range<=0)return 0;
            result=(LONG)max(0,min(65535,((LONGLONG)result-cap->LogicalMin)*65535/range));
            if(usage==0x30)d->absoluteX=result;else d->absoluteY=result;
        }
        if(valid)*valid=TRUE;
        return result;
    }
    return 0;
}
static void FilterMouse(DEVICE_CONTEXT* d) {
    USAGE fallback[32];
    PUSAGE usages=d->buttonUsages?d->buttonUsages:fallback;
    const ULONG capacity=d->buttonUsages?d->buttonUsageCapacity:min(d->buttonUsageCapacity,(ULONG)RTL_NUMBER_OF(fallback));
    ULONG count=capacity,i;
    UCHAR buttons=0,mapped;
    MOUSE_INPUT_DATA packet={0};
    LONG x,y,wheel,horizontal;
    BOOLEAN validX=FALSE,validY=FALSE;
    const UCHAR mode=d->mouseModes[d->report[0]];
    const BOOLEAN absolute=(mode&IC_MOUSE_REPORT_ABSOLUTE)!=0 ||
        (d->forceAbsolute && (mode&(IC_MOUSE_REPORT_X|IC_MOUSE_REPORT_Y))!=0);
    if(capacity) {
        const NTSTATUS status=HidP_GetUsages(HidP_Input,9,0,usages,&count,
            d->preparsed,(PCHAR)d->report,d->caps.InputReportByteLength);
        if(status==HIDP_STATUS_SUCCESS && count<=capacity) {
            const UCHAR previous=d->buttonReports[d->report[0]];
            for(i=0;i<count;++i) {
                const USAGE usage=usages[i];
                if(usage>=1 && usage<=8)buttons|=(UCHAR)(1u<<(usage-1));
            }
            for(i=0;i<8;++i)if((previous^buttons)&(1u<<i)) {
                if(buttons&(1u<<i))++d->buttonOwners[i];else --d->buttonOwners[i];
                if(d->buttonOwners[i])d->physicalButtons|=(UCHAR)(1u<<i);
                else d->physicalButtons&=(UCHAR)~(1u<<i);
            }
            d->buttonReports[d->report[0]]=buttons;
        }
    }
    buttons=d->physicalButtons;
    x=FilterAxis(d,1,0x30,&validX);
    y=FilterAxis(d,1,0x31,&validY);
    wheel=FilterAxis(d,1,0x38,NULL);
    horizontal=FilterAxis(d,0x0c,0x238,NULL);
    mapped=buttons;
    for (i = 0; i < 5; ++i) if ((mapped ^ d->buttons) & (1u << i))
        packet.ButtonFlags |= (USHORT)(1u << (2 * i + ((mapped & (1u << i)) ? 0 : 1)));
    if(absolute) {
        // A null or absent axis must never turn a button-only report into a
        // move to the origin. Reuse its last position only on real movement.
        if(validX || validY) {
            packet.Flags=MOUSE_MOVE_ABSOLUTE|MOUSE_VIRTUAL_DESKTOP;
            packet.LastX=validX?max(0,min(65535,x)):d->absoluteX;
            packet.LastY=validY?max(0,min(65535,y)):d->absoluteY;
        }
    } else {
        packet.LastX=x;
        packet.LastY=y;
    }
    if(wheel) {
        packet.ButtonFlags|=MOUSE_WHEEL;
        packet.ButtonData=(USHORT)FilterScaleWheel(wheel,d->flipWheel);
    }
    if(packet.ButtonFlags || packet.LastX || packet.LastY || packet.Flags)FilterDeliver(d,&packet,&packet+1);
    if(horizontal) {
        RtlZeroMemory(&packet,sizeof(packet));
        packet.ButtonFlags=MOUSE_HWHEEL;
        packet.ButtonData=(USHORT)FilterScaleWheel(horizontal,d->flipHorizontal);
        FilterDeliver(d,&packet,&packet+1);
    }
    d->buttons = mapped;
}

static void FilterReaderReady(DEVICE_CONTEXT* d) {
    WdfWaitLockAcquire(gGate,NULL);
    if(!d->readerReady && !d->stop) {
        d->readerReady=TRUE;
        IcDeviceReadyLocked(d);
    }
    WdfWaitLockRelease(gGate);
}

static void FilterReaderOffline(DEVICE_CONTEXT* d) {
    BOOLEAN wasReady;
    WdfWaitLockAcquire(gGate,NULL);
    wasReady=d->readerReady;
    d->readerReady=FALSE;
    WdfWaitLockRelease(gGate);
    if(wasReady)IcDeviceStopping(d);
    RtlZeroMemory(d->keys,sizeof(d->keys));RtlZeroMemory(d->mapped,sizeof(d->mapped));
    RtlZeroMemory(d->keyCounts,sizeof(d->keyCounts));RtlZeroMemory(d->buttonReports,sizeof(d->buttonReports));
    RtlZeroMemory(d->buttonOwners,sizeof(d->buttonOwners));d->physicalButtons=0;
    if(d->keyReports)RtlZeroMemory(d->keyReports,256*32);
    d->buttons=0;d->repeatUsage=0;d->modifiers.ul=0;d->prefix=0;
}

static void FilterFreeReader(DEVICE_CONTEXT* d) {
    if(d->buttonUsages) {ExFreePoolWithTag(d->buttonUsages,FILTER_TAG);d->buttonUsages=NULL;}
    d->buttonUsageCapacity=0;
    if(d->readRequest) {WdfObjectDelete(d->readRequest);d->readRequest=NULL;d->readMemory=NULL;}
    if(d->values) {ExFreePoolWithTag(d->values,FILTER_TAG);d->values=NULL;d->valueCount=0;}
    if(d->keyReports) {ExFreePoolWithTag(d->keyReports,FILTER_TAG);d->keyReports=NULL;}
    if(d->report) {ExFreePoolWithTag(d->report,FILTER_TAG);d->report=NULL;}
    if(d->preparsed) {ExFreePoolWithTag(d->preparsed,FILTER_TAG);d->preparsed=NULL;}
}

static BOOLEAN FilterRetry(DEVICE_CONTEXT* d,ULONG milliseconds) {
    LARGE_INTEGER delay;
    delay.QuadPart=-(LONGLONG)milliseconds*10000;
    return KeWaitForSingleObject(&d->stopEvent,Executive,KernelMode,FALSE,&delay)==STATUS_WAIT_0;
}

static BOOLEAN FilterWaitForClassOpen(DEVICE_CONTEXT* d) {
    PVOID events[2]={&d->stopEvent,&d->configurationEvent};
    while(!InterlockedCompareExchange(&d->stop,0,0)) {
        BOOLEAN connected;
        WdfWaitLockAcquire(gGate,NULL);
        connected=(BOOLEAN)(d->hidFile && IcKeyboardMouseConnected(d));
        WdfWaitLockRelease(gGate);
        if(connected)return TRUE;
        if(KeWaitForMultipleObjects(2,events,WaitAny,Executive,KernelMode,FALSE,NULL,NULL)==STATUS_WAIT_0)
            break;
    }
    return FALSE;
}

static void FilterReadThread(PVOID context) {
    DEVICE_CONTEXT* d=context;
    ULONG retryMs=25,failures=0;
    KeWaitForSingleObject(&d->readerStarted,Executive,KernelMode,FALSE,NULL);
    while(!InterlockedCompareExchange(&d->stop,0,0)) {
        NTSTATUS status;
        ULONG_PTR received=0;
        if(!d->readRequest) {
            status=FilterPrepareReader(d->device);
            InterlockedExchange(&d->readStatus,status);
            if(!NT_SUCCESS(status)) {
                if(FilterRetry(d,retryMs))break;
                retryMs=min(retryMs*2,2000u);
                continue;
            }
            if(!FilterWaitForClassOpen(d))break;
            (void)FilterReserveInputBuffers(d->device,d->hidFile);
            if(d->keyboard) {
                InterlockedExchange(&d->indicatorDirty,TRUE);
                FilterApplyIndicators(d);
            }
        }
        if(d->stop)break;
        status=FilterRead(d,&received);
        if(d->stop)break;
        if(NT_SUCCESS(status) && received!=d->caps.InputReportByteLength)status=STATUS_INVALID_BUFFER_SIZE;
        InterlockedExchange(&d->readStatus,status);
        if(!NT_SUCCESS(status)) {
            // A transport may return disconnection while Windows is still
            // powering the devnode. Honor stop, back off, then rebuild the
            // request and parser if the same generation stays unhealthy.
            if(++failures==4) {
                FilterReaderOffline(d);
                FilterFreeReader(d);
                failures=0;
            }
            if(FilterRetry(d,retryMs))break;
            retryMs=min(retryMs*2,2000u);
            continue;
        }
        retryMs=25;
        failures=0;
        if(d->keyboard) {FilterRepeat(d);FilterKeyboard(d);}
        else FilterMouse(d);
    }
    FilterReaderOffline(d);
    FilterFreeReader(d);
    PsTerminateSystemThread(STATUS_SUCCESS);
}

static void FilterStartLocked(WDFDEVICE device) {
    DEVICE_CONTEXT* d=FilterDevice(device);
    HANDLE thread;
    PETHREAD object=NULL;
    NTSTATUS status;
    OBJECT_ATTRIBUTES attributes;
    if(!d->powered)return;
    if(d->thread) {
        KeSetEvent(&d->configurationEvent,IO_NO_INCREMENT,FALSE);
        return;
    }
    KeClearEvent(&d->configurationEvent);
    KeClearEvent(&d->stopEvent);KeClearEvent(&d->maintenanceEvent);KeClearEvent(&d->readerStarted);
    FilterSetStop(d,0);
    InitializeObjectAttributes(&attributes,NULL,OBJ_KERNEL_HANDLE,NULL,NULL);
    status=PsCreateSystemThread(&thread,THREAD_ALL_ACCESS,&attributes,NULL,NULL,FilterReadThread,d);
    if(NT_SUCCESS(status)) {
        status=ObReferenceObjectByHandle(thread,SYNCHRONIZE,*PsThreadType,KernelMode,(PVOID*)&object,NULL);
        if(NT_SUCCESS(status)) {
            WdfWaitLockAcquire(gGate,NULL);d->thread=object;WdfWaitLockRelease(gGate);
        } else {
            FilterSetStop(d,1);
            KeSetEvent(&d->stopEvent,IO_NO_INCREMENT,FALSE);
        }
        KeSetEvent(&d->readerStarted,IO_NO_INCREMENT,FALSE);
        if(!NT_SUCCESS(status))ZwWaitForSingleObject(thread,FALSE,NULL);
        ZwClose(thread);
    }
    if(!NT_SUCCESS(status)) {
        FilterSetStop(d,1);
        InterlockedExchange(&d->readStatus,status);
        WdfTimerStart(d->retryTimer,WDF_REL_TIMEOUT_IN_MS(500));
    }
}

static void FilterStopLocked(WDFDEVICE device) {
    DEVICE_CONTEXT* d=FilterDevice(device);
    FilterSetStop(d,1);
    WdfTimerStop(d->retryTimer,FALSE);
    if(d->thread) {
        KeSetEvent(&d->stopEvent,IO_NO_INCREMENT,FALSE);
        KeWaitForSingleObject(d->thread,Executive,KernelMode,FALSE,NULL);
        WdfWaitLockAcquire(gGate,NULL);
        ObDereferenceObject(d->thread);d->thread=NULL;
        WdfWaitLockRelease(gGate);
    }
}

static EVT_WDF_TIMER FilterRetryStart;
static VOID FilterRetryStart(WDFTIMER timer) {
    WDFDEVICE device=(WDFDEVICE)WdfTimerGetParentObject(timer);
    DEVICE_CONTEXT* d=FilterDevice(device);
    WdfWaitLockAcquire(d->stateLock,NULL);
    FilterStartLocked(device);
    WdfWaitLockRelease(d->stateLock);
}

NTSTATUS FilterInitialize(WDFDEVICE device,BOOLEAN keyboard) {
    DEVICE_CONTEXT* d=FilterDevice(device);
    WDF_TIMER_CONFIG timer;
    WDF_OBJECT_ATTRIBUTES attributes;
    d->keyboard=keyboard;
    d->info.type=keyboard?IC_TYPE_KEYBOARD:IC_TYPE_MOUSE;
    d->info.instance_token=d->id;
    d->mouseAttributes.MouseIdentifier=MOUSE_HID_HARDWARE;
    d->mouseAttributes.NumberOfButtons=2;
    d->mouseAttributes.InputDataQueueLength=100*sizeof(MOUSE_INPUT_DATA);
    d->keyboardAttributes.KeyboardIdentifier.Type=0x51;
    d->keyboardAttributes.KeyboardMode=1;
    d->keyboardAttributes.NumberOfFunctionKeys=12;
    d->keyboardAttributes.NumberOfIndicators=3;
    d->keyboardAttributes.NumberOfKeysTotal=101;
    d->keyboardAttributes.InputDataQueueLength=100*sizeof(KEYBOARD_INPUT_DATA);
    d->keyboardAttributes.KeyRepeatMinimum.Rate=2;
    d->keyboardAttributes.KeyRepeatMinimum.Delay=250;
    d->keyboardAttributes.KeyRepeatMaximum.Rate=30;
    d->keyboardAttributes.KeyRepeatMaximum.Delay=1000;
    d->indicatorMask=KEYBOARD_NUM_LOCK_ON|KEYBOARD_CAPS_LOCK_ON|KEYBOARD_SCROLL_LOCK_ON;
    d->typematic.Rate=30;
    d->typematic.Delay=500;
    d->stop=1;
    d->readStatus=STATUS_DEVICE_NOT_READY;
    InitializeListHead(&d->readFiles);
    KeInitializeEvent(&d->readDone,SynchronizationEvent,FALSE);
    KeInitializeEvent(&d->readerStarted,NotificationEvent,FALSE);
    KeInitializeEvent(&d->configurationEvent,SynchronizationEvent,FALSE);
    KeInitializeEvent(&d->stopEvent,NotificationEvent,FALSE);
    KeInitializeEvent(&d->maintenanceEvent,SynchronizationEvent,FALSE);
    WDF_TIMER_CONFIG_INIT(&timer,FilterRetryStart);
    timer.AutomaticSerialization=FALSE;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject=device;
    attributes.ExecutionLevel=WdfExecutionLevelPassive;
    return WdfTimerCreate(&timer,&attributes,&d->retryTimer);
}

NTSTATUS FilterPrepare(WDFDEVICE device,WDFCMRESLIST raw,WDFCMRESLIST translated) {
    UNREFERENCED_PARAMETER(raw);
    UNREFERENCED_PARAMETER(translated);
    return IcRegister(FilterDevice(device));
}

NTSTATUS FilterStart(WDFDEVICE device) {
    DEVICE_CONTEXT* d=FilterDevice(device);
    WdfWaitLockAcquire(d->stateLock,NULL);
    d->powered=TRUE;
    WdfWaitLockAcquire(gGate,NULL);IcTrialResumeLocked();WdfWaitLockRelease(gGate);
    FilterStartLocked(device);
    WdfWaitLockRelease(d->stateLock);
    return STATUS_SUCCESS;
}

void FilterStop(WDFDEVICE device) {
    DEVICE_CONTEXT* d=FilterDevice(device);
    WdfWaitLockAcquire(d->stateLock,NULL);
    d->powered=FALSE;
    FilterStopLocked(device);
    WdfWaitLockRelease(d->stateLock);
}

NTSTATUS FilterSuspend(WDFDEVICE device) {FilterStop(device);return STATUS_SUCCESS;}
NTSTATUS FilterRelease(WDFDEVICE device,WDFCMRESLIST resources) {
    UNREFERENCED_PARAMETER(resources);
    FilterStop(device);
    IcUnregister(FilterDevice(device));
    return STATUS_SUCCESS;
}

static NTSTATUS FilterIndicators(DEVICE_CONTEXT* d,USHORT flags) {
    PHIDP_BUTTON_CAPS caps;PUCHAR report;USHORT count=d->caps.NumberOutputButtonCaps;const ULONG capacity=count;ULONG i;
    UCHAR done[256]={0};NTSTATUS status=STATUS_SUCCESS;
    if(!d->hidFile || !d->preparsed || !count || !d->caps.OutputReportByteLength)return STATUS_SUCCESS;
    if(!capacity || capacity>512 || d->caps.OutputReportByteLength>4096)return STATUS_NOT_SUPPORTED;
    caps=FilterAllocateButtons(capacity);
    report=ExAllocatePoolZero(NonPagedPoolNx,d->caps.OutputReportByteLength,FILTER_TAG);
    if(!caps || !report) { status=STATUS_INSUFFICIENT_RESOURCES;goto end; }
    status=FilterButtonCaps(d->preparsed,HidP_Output,capacity,caps,&count);if(status!=HIDP_STATUS_SUCCESS)goto end;
    if(count>capacity) { status=STATUS_INVALID_BUFFER_SIZE;goto end; }
    for(i=0;i<capacity && i<count;++i)if(caps[i].UsagePage==8 && !done[caps[i].ReportID]) {
        USHORT j;
        done[caps[i].ReportID]=1;
        status=HidP_InitializeReportForID(HidP_Output,caps[i].ReportID,d->preparsed,(PCHAR)report,d->caps.OutputReportByteLength);
        if(status!=HIDP_STATUS_SUCCESS)goto end;
        for(j=0;j<capacity && j<count;++j)if(caps[j].UsagePage==8 && caps[j].ReportID==caps[i].ReportID) {
            USAGE usages[3];ULONG n=0,k;
            for(k=1;k<=3;++k) {
                USHORT bit=k==1?KEYBOARD_NUM_LOCK_ON:k==2?KEYBOARD_CAPS_LOCK_ON:KEYBOARD_SCROLL_LOCK_ON;
                if((flags&bit) && (caps[j].IsRange ? k>=caps[j].Range.UsageMin&&k<=caps[j].Range.UsageMax : k==caps[j].NotRange.Usage))usages[n++]=(USAGE)k;
            }
            if(n) { status=HidP_SetUsages(HidP_Output,8,caps[j].LinkCollection,usages,&n,d->preparsed,(PCHAR)report,d->caps.OutputReportByteLength);if(status!=HIDP_STATUS_SUCCESS)goto end; }
        }
        status=FilterTransfer(d,TRUE,report,d->caps.OutputReportByteLength,WDF_REL_TIMEOUT_IN_SEC(1),NULL);
        if(!NT_SUCCESS(status))goto end;
    }
    status=STATUS_SUCCESS;
end:
    if(caps)ExFreePoolWithTag(caps,FILTER_TAG);if(report)ExFreePoolWithTag(report,FILTER_TAG);
    return status;
}
static void FilterApplyIndicators(DEVICE_CONTEXT* d) {
    if(InterlockedExchange(&d->indicatorDirty,FALSE))
        (void)FilterIndicators(d,(USHORT)InterlockedCompareExchange(&d->indicators,0,0));
}

NTSTATUS FilterDeviceControl(WDFDEVICE device,PIRP irp) {
    const ULONG code=IoGetCurrentIrpStackLocation(irp)->Parameters.DeviceIoControl.IoControlCode;
    switch(code) {
    case IOCTL_INTERNAL_KEYBOARD_CONNECT:
    case IOCTL_INTERNAL_KEYBOARD_DISCONNECT:
    case IOCTL_INTERNAL_MOUSE_CONNECT:
    case IOCTL_INTERNAL_MOUSE_DISCONNECT:
    case IOCTL_KEYBOARD_QUERY_ATTRIBUTES:
    case IOCTL_KEYBOARD_QUERY_TYPEMATIC:
    case IOCTL_KEYBOARD_SET_TYPEMATIC:
    case IOCTL_KEYBOARD_QUERY_INDICATORS:
    case IOCTL_KEYBOARD_SET_INDICATORS:
    case IOCTL_KEYBOARD_QUERY_INDICATOR_TRANSLATION:
    case IOCTL_MOUSE_QUERY_ATTRIBUTES:
        IoSkipCurrentIrpStackLocation(irp);
        return WdfDeviceWdmDispatchPreprocessedIrp(device,irp);
    default:
        // HIDClass owns the external HID contract, including feature requests.
        // Send the original IRP before any queue or buffer conversion so its
        // caller context, file object, access checks and completion survive.
        IoSkipCurrentIrpStackLocation(irp);
        return IoCallDriver(WdfDeviceWdmGetAttachedDevice(device),irp);
    }
}

void FilterInternal(WDFQUEUE queue, WDFREQUEST request, size_t outSize, size_t inSize, ULONG code) {
    DEVICE_CONTEXT* d = FilterDevice(WdfIoQueueGetDevice(queue)); PVOID buffer;
    NTSTATUS status = STATUS_NOT_SUPPORTED; size_t bytes = 0;
    UNREFERENCED_PARAMETER(outSize); UNREFERENCED_PARAMETER(inSize);
    if((code==IOCTL_INTERNAL_KEYBOARD_CONNECT || code==IOCTL_INTERNAL_MOUSE_CONNECT ||
        code==IOCTL_INTERNAL_KEYBOARD_DISCONNECT || code==IOCTL_INTERNAL_MOUSE_DISCONNECT) && WdfRequestGetRequestorMode(request)!=KernelMode) {
        WdfRequestComplete(request,STATUS_ACCESS_DENIED);return;
    }
    WdfWaitLockAcquire(d->stateLock,NULL);
    if (code == IOCTL_INTERNAL_KEYBOARD_CONNECT || code == IOCTL_INTERNAL_MOUSE_CONNECT) {
        if(code!=(ULONG)(d->keyboard?IOCTL_INTERNAL_KEYBOARD_CONNECT:IOCTL_INTERNAL_MOUSE_CONNECT))status=STATUS_INVALID_DEVICE_REQUEST;
        else {
            status=WdfRequestRetrieveInputBuffer(request,sizeof(CONNECT_DATA),&buffer,NULL);
            if(NT_SUCCESS(status)) {
                const CONNECT_DATA* incoming=buffer;
                if(!incoming->ClassService || !incoming->ClassDeviceObject)status=STATUS_INVALID_PARAMETER;
                else if(d->connection.ClassService &&
                    (d->connection.ClassDeviceObject!=incoming->ClassDeviceObject ||
                     d->connection.ClassService!=incoming->ClassService))status=STATUS_SHARING_VIOLATION;
                else {
                    WdfWaitLockAcquire(gGate,NULL);d->connection=*incoming;WdfWaitLockRelease(gGate);
                    FilterStartLocked(d->device);
                }
            }
        }
    } else if (code == IOCTL_INTERNAL_KEYBOARD_DISCONNECT || code == IOCTL_INTERNAL_MOUSE_DISCONNECT) {
        if(code==(ULONG)(d->keyboard?IOCTL_INTERNAL_KEYBOARD_DISCONNECT:IOCTL_INTERNAL_MOUSE_DISCONNECT)) {
            FilterStopLocked(d->device);
            WdfWaitLockAcquire(gGate,NULL);RtlZeroMemory(&d->connection,sizeof(d->connection));WdfWaitLockRelease(gGate);
            status=STATUS_SUCCESS;
        }
    } else if (code==IOCTL_KEYBOARD_QUERY_ATTRIBUTES && d->keyboard) {
        status=WdfRequestRetrieveOutputBuffer(request,sizeof(d->keyboardAttributes),&buffer,NULL);
        if(NT_SUCCESS(status)) {
            WdfWaitLockAcquire(gGate,NULL);
            *(PKEYBOARD_ATTRIBUTES)buffer=d->keyboardAttributes;
            WdfWaitLockRelease(gGate);
            bytes=sizeof(d->keyboardAttributes);
        }
    } else if(code==IOCTL_KEYBOARD_QUERY_TYPEMATIC && d->keyboard) {
        status=WdfRequestRetrieveOutputBuffer(request,sizeof(d->typematic),&buffer,NULL);
        if(NT_SUCCESS(status)) { WdfSpinLockAcquire(gLock);*(PKEYBOARD_TYPEMATIC_PARAMETERS)buffer=d->typematic;WdfSpinLockRelease(gLock);bytes=sizeof(d->typematic); }
    } else if(code==IOCTL_KEYBOARD_SET_TYPEMATIC && d->keyboard) {
        status=WdfRequestRetrieveInputBuffer(request,sizeof(d->typematic),&buffer,NULL);
        if(NT_SUCCESS(status)) {
            PKEYBOARD_TYPEMATIC_PARAMETERS p=buffer;
            if(p->Rate<2 || p->Rate>30 || p->Delay<250 || p->Delay>1000)status=STATUS_INVALID_PARAMETER;
            else { WdfSpinLockAcquire(gLock);d->typematic=*p;WdfSpinLockRelease(gLock); }
        }
    } else if(code==IOCTL_KEYBOARD_QUERY_INDICATORS && d->keyboard) {
        status=WdfRequestRetrieveOutputBuffer(request,sizeof(KEYBOARD_INDICATOR_PARAMETERS),&buffer,NULL);
        if(NT_SUCCESS(status)) { PKEYBOARD_INDICATOR_PARAMETERS p=buffer;p->UnitId=0;p->LedFlags=(USHORT)InterlockedCompareExchange(&d->indicators,0,0);bytes=sizeof(*p); }
    } else if(code==IOCTL_KEYBOARD_SET_INDICATORS && d->keyboard) {
        status=WdfRequestRetrieveInputBuffer(request,sizeof(KEYBOARD_INDICATOR_PARAMETERS),&buffer,NULL);
        if(NT_SUCCESS(status)) {
            InterlockedExchange(&d->indicators,((PKEYBOARD_INDICATOR_PARAMETERS)buffer)->LedFlags);
            InterlockedExchange(&d->indicatorDirty,TRUE);
            KeSetEvent(&d->maintenanceEvent,IO_NO_INCREMENT,FALSE);
        }
    } else if(code==IOCTL_KEYBOARD_QUERY_INDICATOR_TRANSLATION && d->keyboard) {
        static const USHORT indicators[3]={KEYBOARD_CAPS_LOCK_ON,KEYBOARD_NUM_LOCK_ON,KEYBOARD_SCROLL_LOCK_ON};
        static const USHORT scanCodes[3]={0x3a,0x45,0x46};
        USHORT mask,count=0;
        ULONG i,length;
        WdfWaitLockAcquire(gGate,NULL);
        mask=d->indicatorMask;
        WdfWaitLockRelease(gGate);
        for(i=0;i<RTL_NUMBER_OF(indicators);++i)if(mask&indicators[i])++count;
        length=FIELD_OFFSET(KEYBOARD_INDICATOR_TRANSLATION,IndicatorList)+count*sizeof(INDICATOR_LIST);
        status=WdfRequestRetrieveOutputBuffer(request,length,&buffer,NULL);
        if(NT_SUCCESS(status)) {
            PKEYBOARD_INDICATOR_TRANSLATION result=buffer;
            RtlZeroMemory(result,length);
            result->NumberOfIndicatorKeys=count;
            count=0;
            for(i=0;i<RTL_NUMBER_OF(indicators);++i)if(mask&indicators[i]) {
                result->IndicatorList[count].MakeCode=scanCodes[i];
                result->IndicatorList[count++].IndicatorFlags=indicators[i];
            }
            bytes=length;
        }
    } else if (code==IOCTL_MOUSE_QUERY_ATTRIBUTES && !d->keyboard) {
        status=WdfRequestRetrieveOutputBuffer(request,sizeof(d->mouseAttributes),&buffer,NULL);
        if(NT_SUCCESS(status)) {
            WdfWaitLockAcquire(gGate,NULL);
            *(PMOUSE_ATTRIBUTES)buffer=d->mouseAttributes;
            WdfWaitLockRelease(gGate);
            bytes=sizeof(d->mouseAttributes);
        }
    }
    WdfWaitLockRelease(d->stateLock);WdfRequestCompleteWithInformation(request, status, bytes);
}
