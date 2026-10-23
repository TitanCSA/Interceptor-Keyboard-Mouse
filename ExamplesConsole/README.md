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

Compile with `powershell -File .\Build.ps1 -Target ExamplesConsole` from the project root. Every example uses `Interceptor/Interceptor.h` and `Interceptor/Interceptor.cpp`.
