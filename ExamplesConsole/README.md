# Keyboard / mouse C++ examples

Install this product driver first, then run one example as administrator. Each example checks for its own driver service at startup. The library creates its capture automatically. Closing the application releases its context and restores passthrough. The demo lasts one hour from the first successful capture per Windows boot, shared by all applications using this edition.

| Executable | Purpose |
| --- | --- |
| `01_Echange_H_J` | Swap H and J |
| `02_Ctrl_H_Hello` | Ctrl+H types hello |
| `03_Inverse_Souris_Y` | Invert relative mouse Y |
| `05_Clavier_Bloque_Affichage` | Display and block keyboard input |
| `06_Souris_Bloquee_Affichage` | Display and block mouse input |
| `08_Clavier_Minimal` | Minimal receive, change and forward loop |
| `09_Mouse_Move_Horizontal` | Send a signed relative mouse X movement through the driver |

Compile with `powershell -File .\Build.ps1 -Target ExamplesConsole` from the project root. Every example uses `Interceptor/Interceptor.h` and `Interceptor/Interceptor.cpp`.

## Move the mouse using raw relative X input

Extract the examples ZIP and run `ExamplesConsole/09_Mouse_Move_Horizontal.exe`
as administrator after installing this edition's driver. When building from
source, the EXE is in `bin/Native/`. Type `300` and Enter to send X = +300, or
`-485` and Enter to send X = -485. `0` does nothing. Esc, `q` and Enter, or Ctrl+C
exits.

The value is sent directly as a relative mouse input through Interceptor:

```cpp
Interceptor::Mouse_Input input{};
input.device = mouse.id;        // A mouse returned by Get_Devices().
auto& state = input.stroke;
state.x = 300;                 // Relative X; Y, buttons and flags remain zero.
auto result = Interceptor::Send_Input(input);
```

`state` is a local reference to the SDK's `stroke` field. There is no cursor
position query, absolute coordinate conversion or screen-bound clamping. These
are raw movement counts, not a guaranteed number of screen pixels: Windows and
applications apply their own sensitivity/acceleration. The example also checks
the send result and handles queue backpressure.

Physical input is forwarded while you enter commands. The console and driver
share an event-driven loop with no idle busy polling. Run one capture app at a
time. The installed demo's one-hour allowance still applies; no driver update
is required. The compiled EXE is distributed inside the examples ZIP.

[Source](09_Mouse_Move_Horizontal.cpp)
