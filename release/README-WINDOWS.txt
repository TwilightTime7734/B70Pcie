B70Pcie v1.1 - Windows x64

This native utility reads and changes the PCIe Gen4 Downgrade firmware setting
on Intel Arc Pro B70 GPUs with PCI vendor/device ID 8086:E223.
No separate igsc.exe, igsc.dll, or MeTee DLL is required.

Unzip the package. Open an Administrator Command Prompt in the extracted folder.

Commands:
  B70Pcie.exe list
  B70Pcie.exe status
  B70Pcie.exe status 1
  B70Pcie.exe disable 1
  B70Pcie.exe disable
  B70Pcie.exe enable 1

Device indexes start at 0 and match the [0], [1], etc. printed by list/status.
The index includes unsupported devices. Check the selected card's PCI address
before typing YES. Indexes can change after hardware or driver changes.

disable 1:
  Clears the Pending downgrade flag only on device [1].

disable with no index:
  Selects the first supported B70 with Pending ENABLED in the current list order.
  Changes at most one card. Repeat the command to process the remaining cards.
  Skips cards already queued as Pending Disabled, even if Current is still
  ENABLED until a cold shutdown/power-on. If all are already queued off, sends
  no write. A status-read error stops selection without changing another card.

enable 1:
  Sets the Pending downgrade flag on device [1]. enable without an index works
  only when exactly one supported B70 is detected.

The program requires Administrator access and YES confirmation before writing.
It reads the configuration again, preserves the existing 4-byte Pending field,
changes only bit 1, sends one write, and verifies the full Pending field.

Current is the setting active now. Pending is the saved request. A full cold
shutdown/power-on may be required for Current to match Pending. Disabling the
flag allows normal PCIe link training and does not guarantee a Gen5 link.

Validation:
  The owner reported testing enable/disable, status, indexed selection, and
  automatic selection on one physical B70 on 2026-10-23.
  Multiple-card behavior passed simulated CLI tests. Testing with several
  physical B70s remains outstanding. GPUs absent from IGSC enumeration cannot
  be accessed by this utility.

Checksums: SHA256SUMS.txt lists the SHA-256 hashes of the included files.
Licenses: see LICENSE.txt, THIRD_PARTY_NOTICES.md, and the Intel license files.

Project: https://github.com/TwilightTime7734/B70Pcie
Independent community project; not affiliated with or endorsed by Intel.
