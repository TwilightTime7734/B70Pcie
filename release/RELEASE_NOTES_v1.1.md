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

## IT administrator note: downgrade at boot

The owner reports that the B70 initially negotiates **Gen5 x16** at power-on,
but can come up at **Gen4 x16 after Windows boots** when the persistent PCIe
Gen4 Downgrade flag is enabled. On the owner's workstation, clearing that flag
before shutting Windows down has consistently allowed the next startup to run
at Gen5 x16.

For systems showing the same behavior, an elevated end-of-session or
pre-shutdown check can leave each affected card at **Pending: Disabled** before
power-off. `disable` clears the downgrade flag; `enable` turns the Gen4 downgrade
setting on. Verify the actual link speed after the next startup. This is an
owner-tested troubleshooting workaround, not a guarantee of Gen5 operation on
every system. On multi-card systems, check each card; an unindexed `disable`
changes at most one card per invocation.

## Download

Download **[B70Pcie.exe](https://github.com/TwilightTime7734/B70Pcie/releases/download/v1.1/B70Pcie.exe)** from the [v1.1 release](https://github.com/TwilightTime7734/B70Pcie/releases/tag/v1.1).

SHA-256 of the published executable:

```text
2942fe2aad47a7dbc5206ce64f347a959aeac00ae9da989cbb8140a899fcb276  B70Pcie.exe
```

The repository also includes an optional ZIP packaging script; see
[release packaging](README.md). The older v1.0 and manual downloads remain
available for reference.
