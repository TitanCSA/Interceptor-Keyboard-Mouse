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
| `09_Mouse_Move_Horizontal` | Enter a signed pixel distance to move the desktop cursor horizontally |

Compile with `powershell -File .\Build.ps1 -Target ExamplesConsole` from the project root. Every example uses `Interceptor/Interceptor.h` and `Interceptor/Interceptor.cpp`.

## Move the mouse by a number of pixels

Run `bin/Native/09_Mouse_Move_Horizontal.exe` as administrator in an interactive
Windows console. Type `300` and press Enter to move right by 300 pixels, or
`-485` to move left by 485 pixels. Enter another number to move again; `0` does
nothing. Press Esc, type `q` and Enter, or use Ctrl+C to exit.

The example sends a `Mouse_Input` with `Send_Input`, using the first captured
mouse. It calculates an absolute destination from the current cursor position,
preserves Y and uses physical desktop pixels across monitors. Unlike relative
counts, the distance is not scaled by Windows pointer acceleration. Screen
edges, monitor gaps or an application's cursor confinement can limit movement.
The printed destination is the requested target, not a measurement of the
resulting cursor position. Do not move the physical mouse during the command
when checking the distance.

Captured keyboard and mouse input is forwarded unchanged. The console and
driver share an event-driven loop, so typing a number does not block physical
input and idle does not busy-poll. Close other capture apps first. The installed
demo's one-hour allowance still applies. No driver update is needed.

[Source](09_Mouse_Move_Horizontal.cpp) |
[Windows mouse packet flags](https://learn.microsoft.com/en-us/windows/win32/api/ntddmou/ns-ntddmou-mouse_input_data)
