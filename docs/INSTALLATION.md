# Installation, diagnostics and recovery

This edition includes its own verified Microsoft-signed demo driver. The release
installers work without enabling test signing or importing a development certificate.

Use `InterceptorSetup.exe` or
`InterceptorSetupConsole.exe /install`. Both use the same package and association
engine. `/uninstall` restores this product's associations and removes its packages.
Exit code `3010` means a Windows restart is required. After installation and a
restart, reopen the same installer or rerun `/install` to finish associations.

Installation and capture require administrator rights. Merely compiling an
example does not install a driver. Each example checks for the correct product
service and gives installation instructions if it is absent.

The installer lists only this edition's device families plus its control endpoint.
A control endpoint is the application's connection to the driver, not a physical
keyboard or mouse. A HID collection can use a driver in its parent stack,
so a blank service field is not necessarily an installation failure. An associated
filter alone is not proof that input capture or every physical control works.

**Diagnose** checks one recognized device at a time, reports installation/API
state and the checks that can run automatically, and provides copy/export actions.
No observed input means physical interception/modification is unverified, rather
than a fabricated success or proof that the device is unsupported.

The keyboard/mouse edition and gamepad/controller edition have independent
services and state. Their installers leave the other edition's filters, driver
packages and recovery configuration intact. If original Interceptor already
captures an overlapping family, remove that overlapping installation first;
the split installer refuses ambiguous stacking.

Close capture applications before uninstalling. Physical **Ctrl+Alt+Delete**
revokes active captures and returns input to Windows. The product-specific
recovery binaries are embedded in both installers. Their startup marker and
recovery state use the product's own identity; see the installer status message.
Use a normal Windows restart after recovery when requested.

Diagnostics and exports can contain PnP instance IDs and controller protocol
metadata. Remove serial numbers before sharing a report publicly.

## Setup safeguards and logs

Both installers verify the embedded package before changing Windows and serialize
maintenance across the Interceptor editions. They check authoritative PnP status
twice before reporting a device as started. A device already disabled before setup
is preserved and reported as unverified. Restart-required states are distinguished
from startup failures, with the instance ID, problem code and failed stage in the log.

If an operation fails, rollback restores this product's saved bindings. Driver
packages and recovery tools remain available while restoration still needs a
restart. Uninstall completes pending removal after the next normal Windows boot;
it does not force-remove a package still referenced by a device. Windows commands
have bounded deadlines. After a timeout, restart Windows before retrying, since a
device operation may still be settling.

Only this product's associations, packages and recovery state are removed.
Keyboard/Mouse and Gamepad/Controller can coexist; the combined Interceptor edition
must be removed before installing either split edition, and vice versa.

| Exit code | Action |
| --- | --- |
| `0` | Completed |
| `3010` | Restart Windows, then reopen setup if installation is pending |
| `1618` | Another setup/recovery operation is active; wait and retry |
| `1638` | Uninstall the conflicting Interceptor edition and restart |
| `1460` | A Windows command timed out; keep recovery files and restart before retrying |
| Other nonzero | Inspect the exact failing stage and numeric error in the log |

The output includes the full transcript path under this edition's
`%ProgramData%` directory. `state.json` records the current maintenance phase;
`setup-devices.json` records bounded PnP verification history. The GUI can export
diagnostics. Normal installation and uninstallation never restart Windows
automatically; restart only when requested.
