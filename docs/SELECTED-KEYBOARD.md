# A spare keyboard without changing the main keyboard

Use this workflow when a PC application should handle one physical keyboard
separately, for example a spare keyboard used as a macro pad. It does not save a
new layout into the keyboard's firmware. The mapping exists while your program
and this edition's driver are running.

1. Install the matching edition and run your capture application elevated.
2. Read `Get_Devices()` and identify the intended keyboard in the current capture.
   Source IDs are family-local; tokens belong to that capture. They are not
   permanent hardware serial numbers. Re-check selection after reconnection or a
   new capture; do not silently choose a replacement device.
3. Receive a batch with `Get_Input_With_Timeout()`. Match the selected source before
   changing or suppressing any keyboard entry.
4. For a macro, track pressed keys. Trigger on the first key-down, avoid unwanted
   repeat triggers, and suppress the corresponding key-up if you suppressed its
   key-down. Leave unrelated keyboard/mouse entries unchanged.
5. Forward the batch with `Send_Input()`, including every unchanged entry. Finish
   that batch before another read and handle errors as described in the API guide.

Example behavior to test: the spare pad's `1` triggers your action, while `1` on
the main keyboard still types normally. Confirm both presses and releases, key
repeat, disconnect/reconnect and recovery on an owned test setup.

## AutoHotkey and hardware-specific questions

Receiving Raw Input identifies sources; it does not by itself suppress their
original input globally. Regular global hotkeys also do not establish per-device
selection. This repository provides a native C++ SDK, not a drop-in AHK wrapper or
the API of another Interception project. Any AHK integration needs its own wrapper
and matching driver/API review.

No MKESPN K809 compatibility claim follows from a request on a forum. Check that
the actual device exposes a supported, independently identifiable source before
offering a mapping. A broken vendor configuration program is a separate issue.

[API and capture ownership](API.md) · [access and trust](SECURITY-MODEL.md)
