# C++ API

Use this edition's `Interceptor.h` and compile `Interceptor.cpp` exactly once.
Windows x64, C++17, `namespace Interceptor`; run capture applications elevated.
The SDK connects only to this product's own driver endpoint.

## Initialization and input

| Function | Purpose |
| --- | --- |
| `Init_AllDevices()` | Associate this edition with supported present devices; returns `ready`, `reboot_required`, `error` and association details |
| `Get_Input_With_Timeout(timeout = INFINITE, stopEvent = nullptr)` | Create/reuse the automatic capture, wait, and return a mixed batch for this edition's families |
| `Send_Input(inputs)` | Forward a mixed batch, event, typed input or typed container; infer its type and size |
| `Get_Context()` | Snapshot with capture token, devices, capabilities, trial and query errors |
| `Get_Devices()` | Snapshot of devices belonging to this capture |
| `Get_Device_Details(device.token)` | Query this source using its capture-scoped string token |
| `Get_descriptor(device.token)` | Identity and applicable transport/parsed HID metadata |
| `Get_Report_Descriptor(device.token)` | Raw HID report descriptor where accessible; otherwise an explicit error or non-applicable result |
| `Get_Trial_Status()` | Read-only two-hour allowance status; does not start it |

`Get_Context()` and `Get_Devices()` also create the automatic capture when needed;
the first successful creation starts the trial. `Init_AllDevices()` and
`Get_Trial_Status()` do not start that allowance.

Family-specific reads available in this edition:

- `Get_Keyboard_Input()`
- `Get_Mouse_Input()`

Each direct read has `(deviceId = 0, consume = true)` and returns an independent
`std::vector`. ID `0` selects all devices of that family; `false` peeks without
removing queued input. Peeking and then sending does not consume the queued original.
Device IDs are family-local and are not VID/PID or XInput player slots.

## Receive, read, forward

```cpp
const auto init = Interceptor::Init_AllDevices();
if (!init.ready) {
    // Handle init.error or restart Windows if init.reboot_required.
    return;
}
auto inputs = Interceptor::Get_Input_With_Timeout(1000);
for (auto& event : inputs) {
    // event.type: family; event.device: source ID; event.device_info: identity.
    // event.Get<The_Input_Type>() returns a typed pointer or nullptr.
    // Change typed fields before sending, or set event.blocked = true.
}
const auto sent = Interceptor::Send_Input(inputs);
// Check sent.error and sent.accepted.
```

Timeout is in milliseconds: `0` polls once, `INFINITE` waits on Windows events.
Timeout or a signaled `stopEvent` returns an empty batch. Cancellation stops this
wait; it does not release an existing automatic capture. Closing the application
releases its capture. The timeout limits waiting for input, not initial context setup.
Driver errors and capture revocation throw `std::system_error`; handle exceptions
in a complete application. A revoked wait can report `ERROR_OPERATION_ABORTED`;
query `Get_Trial_Status()` to distinguish demo expiry from other revocation.

Receiving consumes input. A destroyed batch does not forward its entries.
Forward untouched families as well as changed input. A batch borrows reusable
storage, is noncopyable and stays on its creating thread. Its entries, typed
pointers and `device_info` are valid only until its destruction. Finish the batch
before another receive; another concurrent/live batch is rejected with `ERROR_BUSY`.

`Send_Input` returns `{ accepted, error }`; success is `error == 0`.
When a batch has a partially accepted prefix, retry the same live batch to send
only remaining entries. The SDK does not retry forever automatically. For a typed
vector, remove the accepted prefix yourself before retrying. Preserve per-family
ordering. Mixed batches do not promise a global timestamp order across transports.

For `Keyboard_Input`, read/change `stroke.make_code` (scan code) and preserve the
press/release and extended-key flags in `stroke.flags`. For `Mouse_Input`, use
`stroke.x`, `stroke.y` and `stroke.button_flags`; check relative/absolute flags.

Tokens identify a source within its current capture; do not persist them as stable
hardware serial numbers. Details and descriptors can contain instance IDs.
The first successful capture starts the shared per-edition two-hour demo allowance.
After revocation, restart your application to capture again if that allowance
permits it. A normal Windows restart renews the allowance.
## Add the two files to your application

Compile `Interceptor/Interceptor.cpp` exactly once and add `Interceptor/` to your
include path. Enable C++17, `UNICODE`, `_UNICODE` and `NOMINMAX`; link the Windows
libraries `setupapi`, `newdev`, `cfgmgr32`, `hid`, `advapi32`, `user32`, `comctl32`
and `ole32`. The supplied CMake target `Interceptor::SDK` applies these settings.
No prebuilt SDK `.lib` or DLL is required. Use the complete console examples as
standalone starting points.