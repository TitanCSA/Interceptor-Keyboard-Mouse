# Input access, signing and trust

## What an authorized capture application can do

For ordinary software-generated keystrokes, consider Windows `SendInput` first.
Observing a device through Raw Input or handling shortcuts inside your own app
also does not require this filter. This SDK addresses a different need: changing
or suppressing supported physical-device events in their input path. Do not add
a kernel dependency just to trigger an ordinary application shortcut. `SendInput`
has its own integrity-level and receiver limitations; see [Microsoft's reference](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput).

This is a third-party kernel input filter with an elevated user-mode SDK client.
The demo also uses its resident boot-clock driver. A capture application can read
the captured keyboard/mouse events, change or suppress them, and forward others.
That access can expose sensitive keystrokes. Install only on systems you control
and run capture applications you trust.

An example that checks "only this spare keyboard" implements an application rule.
It is not a security barrier protecting the main keyboard from a malicious capture
application. Do not describe device selection as isolation from all other software.
Administrators and kernel code are outside such an application-level boundary.

Review each capture application's storage, logging and network behavior before
using it with sensitive input. The ability to capture events is not proof that a
particular application records or transmits them, or proof that it cannot do so.
Functional input tests do not establish either security claim.

## What Microsoft signing means here

The owner submits the driver package through Microsoft's Partner Center hardware
dashboard. This is not a Microsoft-authored product. A Microsoft hardware-publisher
signature is distinct from authorship, independent security review, and WHQL/HLK
certification. No additional certification or independent audit is claimed here.
Installer and example EXEs have their own signature status.

The SDK/example/installer source is public; demo/evaluation driver source is documented in [driver/README.md](../driver/README.md) and the
driver binaries have separate terms. Reviewing the public SDK does not audit the
whole stack. See the [published validation scope](DEMO-VALIDATION.md) and
[installation/recovery instructions](INSTALLATION.md). Those results must not be
extended to untested devices, Windows configurations or adversarial conditions.

Before relying on access-control claims, an assessment must cover device access,
IOCTL authorization, malformed input, lifetime/concurrency and recovery. This
document does not announce a new ACL test, a discovered vulnerability, or a
complete public adversarial security assessment.

Microsoft references: [driver signing requirements](https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/code-signing-reqs)
and [driver security checklist](https://learn.microsoft.com/en-us/windows-hardware/drivers/driversecurity/driver-security-checklist).
