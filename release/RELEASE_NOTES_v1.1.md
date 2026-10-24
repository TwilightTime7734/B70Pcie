# B70Pcie v1.1 - Device selection and automatic next-card disabling

This update adds status for every detected device and lets users change the
downgrade setting on a selected B70. It remains a native Windows x64 executable
with no separate IGSC or MeTee DLL required.

## Changes from v1.0

- `status` reports every supported B70, lists unsupported devices, and continues
  after individual read errors.
- `status [index]`, `disable [index]`, and `enable [index]` target the zero-based
  device index printed by `list` and `status`.
- `disable` without an index selects the first supported B70 with Pending
  ENABLED. Repeated runs skip cards already queued as Pending Disabled and
  advance to the next card. Each invocation changes at most one card.
- `enable` without an index preserves the original single-card behavior and
  requires an explicit index when several B70s are detected.
- The selected index and PCI address appear in the confirmation prompt.
- Writes are refused if device enumeration is incomplete.
- Build-tool discovery supports Visual Studio editions and GitHub Windows
  runners; a build/test workflow and release packaging script are included.

Administrator access, supported PCI ID `8086:E223`, Available/Configurable
checks, explicit `YES`, a fresh configuration read, preservation of unrelated
bits, one write attempt, and full Pending read-back verification are retained.

## Validation

On 2026-10-23, the repository owner reported successful enable/disable, status,
explicit targeting, and automatic targeting on one physical B70. The 16
simulated CLI test groups cover multiple cards, repeated selection, errors,
confirmation, and preservation of unrelated bits.

Several physical B70s have not yet been tested together with this update.
A cold shutdown/power-on may be required for Current to match Pending.
Disabling the downgrade flag allows normal link training and does not guarantee
Gen5 on every platform.

## Download package

`B70Pcie-v1.1-win64.zip` contains the executable, usage instructions, third-party
notices and licenses, and `SHA256SUMS.txt`. A separate ZIP checksum accompanies
the archive.

SHA-256 of the owner-tested executable included in the prepared package:

```text
b337a891d3d2fd0d1999a01b4627aa61be100541fabd7aba2a5d41e4efb09cdb  B70Pcie.exe
```

The older v1.0 and manual release downloads remain available for reference.
