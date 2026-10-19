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