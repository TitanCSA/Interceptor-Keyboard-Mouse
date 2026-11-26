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
| `Get_Trial_Status()` | Read-only one-hour allowance status; does not start it |

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
The first successful capture starts the shared per-edition one-hour demo allowance.
After revocation, restart your application to capture again if that allowance
permits it. A normal Windows restart renews the allowance.
## Add the two files to your application

Compile `Interceptor/Interceptor.cpp` exactly once and add `Interceptor/` to your
include path. Enable C++17, `UNICODE`, `_UNICODE` and `NOMINMAX`; link the Windows
libraries `setupapi`, `newdev`, `cfgmgr32`, `hid`, `advapi32`, `user32`, `comctl32`
and `ole32`. The supplied CMake target `Interceptor::SDK` applies these settings.
No prebuilt SDK `.lib` or DLL is required. Use the complete console examples as
standalone starting points.

## Switch the driver for a keyboard or mouse

The C++17 SDK provides these functions in both Demo and Release. Install the
matching update before using the switch. Setup records the original Windows or
manufacturer INF before associating a supported HID keyboard or mouse.

| Function | Behavior |
| --- | --- |
| `Get_Input_Driver_Bindings()` | Lists present Windows HID keyboard and mouse instances without opening input capture. Each result includes the Windows instance ID, active service, INF, estimated `mode`, and the saved `original_requested` preference. Enumeration errors can throw `std::system_error`. |
| `Set_Input_Driver_Mode(instance, true)` | Restores the recorded original binding and pauses automatic Interceptor association for that instance. Use this mode to access settings in compatible manufacturer software. |
| `Set_Input_Driver_Mode(instance, false)` | Associates the selected instance with the installed Interceptor package and resumes automatic association. |

Use the exact `binding.instance` string returned by enumeration. A capture token
does not identify a Windows device instance. Close capture before switching and
run the process as administrator. The call is synchronous and may restart the
device, briefly interrupting its input.

`0` means success. `3010` means Windows must restart before the change is
complete. Other return values are Windows error codes. For example, `740` means
the process needs elevation. A physical device can expose several HID instances.
Switch each required instance separately. If an older installation has no
recorded original manufacturer package, reinstall that package before associating
the device again. See the [command-line examples](../README.md#switching-the-input-driver-for-one-device).
