# Keyboard and mouse compatibility

This edition captures and forwards **keyboard and mouse input only**. It has no
controller engines or controller injection API. Controller device stacks are left
in passthrough even if this filter is manually attached to an inappropriate node.

Supported paths are the Windows keyboard/mouse class callbacks and the inherited
HID mapper profiles. The mapper selects suitable keyboard/mouse report collections;
this is not a list of hardcoded models. Wired USB devices and wireless receivers
can work when Windows exposes a supported keyboard or mouse collection.

Multimedia controls, vendor-specific reports, unusual absolute pointers and
compound devices can expose separate collections. Support for the keyboard/mouse
collection does not establish support for every collection or vendor feature.
Device association, published interfaces and confirmed input capture are different
states; diagnostics report them separately.

This edition does not convert proprietary hardware protocols or emulate a virtual
controller. Use the separate gamepad/controller edition for HID, XUSB/XInputHID and
GIP controller input. Neither product promises every device or connection mode.

The two split editions use separate service, interface, state and recovery identities.
Original Interceptor must be removed from an overlapping stack before association.