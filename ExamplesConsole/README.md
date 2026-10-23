# Interceptor-Keyboard-Mouse console examples

Install this edition with **InterceptorSetup.exe**, restart Windows if requested,
then run **one** example as administrator. Each program checks the driver before
starting and explains its action in English. Close its console or press Ctrl+C to
stop. Physical Ctrl+Alt+Delete is available if an intentional keyboard block
suppresses shortcuts; that example also exits on Escape.

Each `.cpp` is independent: `Interceptor.h`, `Interceptor.cpp`, and standard/Windows
headers are all it needs. There is no shared example runner or behavior header.

| Source / executable | What it does |
| --- | --- |
| [01_Keyboard_Swap_H_J.cpp](01_Keyboard_Swap_H_J.cpp) | Swap H and J, including key presses and releases. |
| [02_Keyboard_Ctrl_H_Hello.cpp](02_Keyboard_Ctrl_H_Hello.cpp) | Ctrl+H types hello. The shortcut H is consumed; other keys keep working. |
| [03_Mouse_Invert_Y.cpp](03_Mouse_Invert_Y.cpp) | Invert mouse Y. X, buttons, wheels and other devices stay unchanged. |
| [05_Keyboard_Block_And_Print.cpp](05_Keyboard_Block_And_Print.cpp) | Print and block keyboard input. Press Escape to stop and restore normal input. |
| [06_Mouse_Block_And_Print.cpp](06_Mouse_Block_And_Print.cpp) | Print and block mouse input. Keyboard and controller input keep working. |
| [08_Keyboard_Minimal.cpp](08_Keyboard_Minimal.cpp) | Minimal example: capture inputs, change H to J, then forward them. |
| [09_Mouse_Move_Horizontal.cpp](09_Mouse_Move_Horizontal.cpp) | Enter relative mouse X counts: 300 moves right, -485 moves left. Enter q to quit. |

## Receive, change, forward

The essential mouse Y operation is easy to follow:

```cpp
auto inputs = Get_Input_With_Timeout(INFINITE);
for (auto& input : inputs)
    if (auto* mouse = input.Get<Mouse_Input>())
        mouse->stroke.y = -mouse->stroke.y;
auto result = Send_Input(inputs);
```

The complete examples also handle absolute coordinates and signed endpoints,
check initialization/send failures, and forward unrelated fields/families.
`INFINITE` sleeps until input arrives; there is no polling, countdown, FPS limit,
per-packet descriptor query or manually managed context. Buffers and capture
lifetime belong to the library. Process exit closes capture in the driver.

Build from the repository root with `./Build.ps1 -Target ExamplesConsole`.
EXEs are written to `bin/Native/`. Download precompiled programs from the release's
**examples ZIP**; the **SDK ZIP** includes their complete sources.

The demo is limited to **one hour per boot**, shared across applications.

## Raw mouse movement

`09_Mouse_Move_Horizontal` sends `Mouse_Input` with `state.x` equal to the number
you type. These are relative input counts, not guaranteed screen pixels; sensitivity
and acceleration are applied by Windows/the game. `state.y`, buttons and flags stay
zero. The console input handle wakes `Get_Input_With_Timeout` when typing, while
physical input is forwarded; no forwarding thread, idle polling or cursor-position API
is needed. Escape or `q` followed by Enter exits.
