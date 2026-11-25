#pragma once
#include "InterceptorProtocol.h"
static __inline int IcKeyboardStrokeSupported(uint16_t makeCode,uint16_t flags) {
    return makeCode<=127 && !(flags&~7u) && (flags&6u)!=6u;
}
static __inline int IcMouseStrokeSupported(uint16_t flags,uint16_t buttons) {
    return !(flags&~0x0Bu) && !(buttons&~0x0FFFu) && (buttons&0x0C00u)!=0x0C00u;
}
static __inline int IcValidKeyboard(const IC_KEYBOARD_INPUT* k) {
    return !k->reserved && !k->padding && !k->stroke.reserved &&
        IcKeyboardStrokeSupported(k->stroke.make_code,k->stroke.flags);
}
static __inline int IcValidMouse(const IC_MOUSE_INPUT* m) {
    return !m->reserved && IcMouseStrokeSupported(m->stroke.flags,m->stroke.button_flags);
}
