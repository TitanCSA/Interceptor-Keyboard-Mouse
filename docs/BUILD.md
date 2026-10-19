# Build the SDK and applications

Use Windows x64, Visual Studio 2022 or Build Tools with **Desktop development
with C++**, a Windows SDK and CMake. The UI uses .NET Framework 4.6, included in
Windows 10/11. The build downloads hash-verified Microsoft .NET reference
assemblies from NuGet only when the local reference assemblies are missing.
No WDK or driver-signing account is needed.

```powershell
.\Build.ps1                         # All user-mode applications and self-tests
.\Build.ps1 -Target SDK             # Optional static C++ library
.\Build.ps1 -Target ExamplesConsole
.\Build.ps1 -Target ExamplesUI
.\Build.ps1 -Target InstallerUI
.\Build.ps1 -Target InstallerConsole
```

Run with `powershell -ExecutionPolicy Bypass -File .\Build.ps1` if script
execution is restricted. `InstallerUI/Build.ps1` and `InstallerConsole/Build.ps1`
also build their respective installer independently.

| Output | Contents |
| --- | --- |
| `bin/Native/` | Console examples and native application components |
| `bin/ExampleUI/` | Standalone input viewer |
| `bin/InstallerUI/` | Standalone graphical installer |
| `bin/InstallerConsole/` | Standalone console installer |
| `bin/SDK/` | Optional static SDK library |

Each build compiles user-mode code only. Installers embed the supplied
Microsoft-signed demo driver package and the precompiled product-specific
recovery binaries from `InstallerUI/Package/`. SHA-256 manifests are verified
before packaging. Driver and recovery binaries are not rebuilt; their source
and private signing material are not included. Building does not install a
driver, capture input or change Windows settings.

The diagnostic worker, installer and viewer self-tests verify user-mode
contracts without installing or capturing. They do not establish that every
physical device or control works. Run the installer's **Diagnose** on the target
PC to check its installation and report any unverified physical input features.

To use only the SDK, add `Interceptor.h` and `Interceptor.cpp` to a C++17
application; no SDK DLL is needed. Alternatively use `add_subdirectory()` and
link `Interceptor::SDK`. See [API setup](API.md).
The two editions use the same namespace; use the library matching your driver
edition and do not link both alternative SDK implementations into one executable.