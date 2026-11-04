#pragma once
// Build identity and immutable family boundary for this separate product.
// This header is also directly testable outside the kernel.
#define IC_PRODUCT_SERVICE L"InterceptorKeyboardMouse"
#define IC_PRODUCT_XUSB_HID_PARENT_VALUE L"InterceptorKeyboardMouseXusbHidParent"
#define IC_PRODUCT_XUSB_PARENT_VALUE L"InterceptorKeyboardMouseXusbParent"
#define IC_PRODUCT_CONTROL_HWID L"ROOT\\INTERCEPTORKEYBOARDMOUSE"
#define IC_PRODUCT_NT_DEVICE L"\\Device\\InterceptorKeyboardMouse"
#define IC_PRODUCT_DOS_DEVICE L"\\DosDevices\\InterceptorKeyboardMouse"
#define IC_PRODUCT_TRIAL_SLOT 1u
#define IC_PRODUCT_TRIAL_KEY L"\\Registry\\Machine\\HARDWARE\\InterceptorKeyboardMouseTrial"
#ifndef IC_PRODUCT_INPUT_MASK
#define IC_PRODUCT_INPUT_MASK 3u
#endif
#ifndef IC_PRODUCT_CONTROLLER_MASK
#define IC_PRODUCT_CONTROLLER_MASK 0u
#endif
#ifndef IC_PRODUCT_HAS_KEYBOARD_MOUSE
#define IC_PRODUCT_HAS_KEYBOARD_MOUSE 1
#endif
#ifndef IC_PRODUCT_HAS_CONTROLLERS
#define IC_PRODUCT_HAS_CONTROLLERS 0
#endif
#define IC_PRODUCT_STREAM_MASK (IC_PRODUCT_INPUT_MASK | (IC_PRODUCT_CONTROLLER_MASK << 2))
#define IC_PRODUCT_HID_GUID {0xbbe712bd,0xcdd8,0x4ddb,{0xa4,0x74,0xb8,0xf7,0x47,0x63,0xfe,0xcf}}
#define IC_PRODUCT_XUSB_GUID {0xf1db5cb4,0x47da,0x4802,{0x88,0x82,0x76,0x7a,0x06,0xc7,0x35,0x52}}
#define IC_PRODUCT_XINPUTHID_GUID {0xa0e7c968,0xe472,0x40ce,{0x86,0x0d,0x1a,0x86,0xe2,0x7a,0x68,0x7f}}
#define IC_PRODUCT_GIP_GUID {0x0f7c63ea,0xb748,0x4c3c,{0x92,0x51,0x5b,0x86,0x6d,0x25,0x31,0x21}}
#define IC_PRODUCT_XUSB_MIRROR_GUID {0x547198fb,0x889e,0x410d,{0x82,0xf1,0x5e,0x0a,0xc4,0x49,0xa7,0xfd}}
static __inline int IcProductTypeAllowed(unsigned int type) {
    return type>=1 && type<=6 && (IC_PRODUCT_STREAM_MASK & (1u<<(type-1)))!=0;
}
static __inline int IcProductCreateMasksAllowed(unsigned int input,unsigned int controllers) {
    return !(input & ~IC_PRODUCT_INPUT_MASK) && !(controllers & ~IC_PRODUCT_CONTROLLER_MASK);
}
// Function numbers are the pointer-free protocol's CTL_CODE function field.
// Unknown/common functions keep the normal control dispatch validation.
static __inline int IcProductIoctlFunctionAllowed(unsigned int fn) {
    switch(fn) {
    case 0xA01:case 0xA03:case 0xA07:
        return (IC_PRODUCT_INPUT_MASK & 1u)!=0;
    case 0xA02:case 0xA04:case 0xA08:
        return (IC_PRODUCT_INPUT_MASK & 2u)!=0;
    case 0xA09:case 0xA0A:case 0xA0B:case 0xA0C:case 0xA0D:
        return (IC_PRODUCT_CONTROLLER_MASK & 1u)!=0;
    case 0xA10:case 0xA11:case 0xA12:case 0xA13:case 0xA14:case 0xA15:
        return (IC_PRODUCT_CONTROLLER_MASK & 2u)!=0;
    case 0xA17:case 0xA18:case 0xA19:case 0xA1A:case 0xA1B:
        return (IC_PRODUCT_CONTROLLER_MASK & 4u)!=0;
    case 0xA20:case 0xA21:case 0xA22:case 0xA23:case 0xA24:case 0xA25:
    case 0xA29:case 0xA2A:case 0xA2B:
        return (IC_PRODUCT_CONTROLLER_MASK & 8u)!=0;
    default:return 1;
    }
}

