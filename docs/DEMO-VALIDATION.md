# Validation scope for demo 1.0.5.4

The embedded keyboard and mouse driver and resident demo clock carry Microsoft
Windows Hardware Compatibility Publisher signatures. The INF files and driver
code match the submitted production candidate. Catalog membership was verified
for each package. The GUI and console installers embed the same signed payload.

Installer, viewer, product and SDK tests pass on Windows x64. The corrected
driver code was tested on a Windows 10 virtual machine after a reboot. The
mouse HID input-buffer query returned the same value as the Windows driver,
and the demo captured and forwarded input from the virtual mouse.

The installer preserves manufacturer-owned driver packages and the replacement
driver forwards external HID requests to Windows HIDClass. Compatibility with
Razer Synapse on physical hardware remains to be verified.

The demo runs for 3,600 seconds per Windows kernel boot, shared across apps.
Use Windows Restart after setup requests it.
