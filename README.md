# B70Pcie

Windows-native utility for inspecting and changing the Intel Arc Pro B70 **PCIe Gen4 Downgrade** firmware configuration.

## Download for Windows

**[Download the latest Windows release](https://github.com/TwilightTime7734/B70Pcie/releases/latest)**

Download **B70Pcie.exe** under **Assets** and run it from an **Administrator terminal**. The native `B70Pcie.exe` includes the required IGSC and MeTee functionality; no separate DLL is required.

See [v1.1 release notes](release/RELEASE_NOTES_v1.1.md) for indexed targeting and automatic next-card selection. The original [manual procedure](docs/MANUAL_PROCEDURE.md) and [manual package](https://github.com/TwilightTime7734/B70Pcie/releases/tag/v0.1-manual) remain available for reference.

## Why this exists

A tested Intel Arc Pro B70 was limited to **PCIe Gen4 x16** even though the card and platform were Gen5-capable. Reading the GPU's GSC/GFSP configuration directly showed the PCIe Gen4 downgrade setting enabled.

After clearing only that configuration bit, verifying the pending state, and performing a cold shutdown/power-on, the same card reported:

```text
PCIe Gen4 Downgrade: disabled
PCIe link:            Gen5 x16 / 32.0 GT/s
```

HWiNFO confirmed the restored **PCIe 5.0 x16** link.

## Commands

```text
B70Pcie.exe list
B70Pcie.exe status
B70Pcie.exe status 1
B70Pcie.exe disable 1
B70Pcie.exe disable
B70Pcie.exe enable 1
```

`list` displays all detected IGSC GSC child devices. `status` displays each
supported B70's configuration; other devices are labeled unsupported. Adding an
index to `status` reads only that device.

Indexes start at **0** and match the `[0]`, `[1]`, etc. printed by `list` and
`status`, including unsupported devices. Thus `disable 1` targets device **[1]**,
not the first B70. Indexes describe the current enumeration and can change after
hardware or driver changes. Check the displayed PCI address before confirming.

## Disabling the downgrade flag

`disable 1` clears the pending downgrade flag only for device [1].

`disable` with no index checks supported B70s in list order and selects the first
card whose **Pending** downgrade flag is enabled. It changes at most one card
per invocation. Repeat the command to advance through the remaining cards.
Cards with Pending already disabled are skipped, even when Current is still
enabled awaiting a cold shutdown/power-on. If all cards already have Pending
disabled, the command succeeds without a write or confirmation prompt.

Automatic selection stops without a write if a checked card cannot be read or
the selected card is not Available/Configurable. It does not silently select a
different card after an error. You can inspect `status` and explicitly target
another supported card by index.

`enable 1` sets the pending downgrade flag on device [1]. `enable` with no index
retains the single-card behavior and refuses when multiple supported B70s exist.

Before a change, the program displays the selected index and PCI address and
requires typing `YES`. It then reads the configuration again, copies the full
four-byte Pending field, changes only bit 1, sends one write, and verifies the
complete Pending field. Writes are refused if device enumeration is incomplete.

Disabling the downgrade flag allows normal PCIe link training; it does not
guarantee Gen5. A cold shutdown/power-on may be required for Current to reflect
Pending. The program does not shut the computer down automatically.

## Build and verification

Install Visual Studio C++ build tools and a Windows SDK, then clone with the pinned dependencies:

```powershell
git clone --recurse-submodules https://github.com/TwilightTime7734/B70Pcie.git
cd B70Pcie
./tests/test.ps1
```

This builds `build/bin/B70Pcie.exe` and runs 16 simulated CLI test groups. `./build.ps1` builds without running the tests. The build discovers installed Visual Studio C++ tools with `vswhere`; it does not require CMake or Ninja.

The tests use simulated IGSC devices and do not access or change GPU firmware. They cover multiple cards, indexed selection, repeated automatic selection, unrelated-bit preservation, confirmation, and failure/read-back handling. The GitHub Actions workflow builds, tests, and prepares a ZIP artifact.

On 2026-10-23, the owner reported successful enable/disable, status checks, indexed targeting, and automatic targeting on one physical B70. **Multiple physical B70s still need validation together.** Devices absent from IGSC enumeration cannot be accessed by this utility.

Use `./release/package.ps1` to prepare the ZIP, usage instructions, licenses, and checksums. See [release packaging](release/README.md).

## What was discovered

Intel's Compute Runtime implements the PCIe downgrade update using the GPU's GSC firmware configuration interface:

- GFSP command `0x10` (`16`) reads the configuration.
- The response contains five 4-byte fields: Available, Current, Configurable, Pending, and Default.
- PCIe Gen4 Downgrade is **bit 1** in those fields.
- GFSP command `0x0F` (`15`) writes a 4-byte configuration request.
- Intel's implementation copies the existing 4-byte Pending field and modifies only bit 1 before writing it back.

The public Intel IGSC API exposes the generic GFSP call used by this project:

```c
igsc_gfsp_heci_cmd(...)
```

## Documentation

- [Manual procedure](docs/MANUAL_PROCEDURE.md) — the exact Windows procedure that was tested successfully.
- [Technical details](docs/TECHNICAL_DETAILS.md) — GFSP layout, command IDs, offsets, and bit handling.
- [Tested results](docs/TESTED_RESULTS.md) — before/write/after values and the confirmed Gen5 x16 result.

## Requirements and safety

- Windows x64 and an Intel graphics driver exposing the GPU's GSC/HECI device.
- Administrator access for configuration reads and writes.
- Supported PCI vendor/device ID: **8086:E223**.

This changes a persistent GPU firmware configuration value. It does not flash a firmware image. Writes preserve the existing Pending field and change only bit 1. The original procedure used Intel IGSC 1.3.1, and the native build retains the pinned IGSC and MeTee source versions documented in [third-party notices](THIRD_PARTY_NOTICES.md).

If Gen5 link training is unstable on a particular platform, forcing the slot back to Gen4 in system BIOS is a practical recovery path.

## Upstream references

This project is based on public interfaces and behavior documented in Intel's open-source projects:

- Intel IGSC: https://github.com/intel/igsc
- Intel Compute Runtime: https://github.com/intel/compute-runtime

Relevant upstream source locations include:

- `intel/igsc/include/igsc_lib.h.in` — public `igsc_gfsp_heci_cmd` API
- `intel/compute-runtime/level_zero/sysman/source/shared/firmware_util/sysman_firmware_util.h` — GFSP command IDs and field offsets
- `intel/compute-runtime/level_zero/sysman/source/api/pci/linux/sysman_os_pci_imp.cpp` — PCIe downgrade read/modify/write behavior

## Disclaimer

This is an independent community project and is not affiliated with or endorsed by Intel. Use firmware-level configuration tools at your own risk.

This repository was developed and released with assistance from **ChatGPT by OpenAI**. ChatGPT helped trace the relevant Intel open-source code paths, organize the testing procedure, and prepare the documentation and GitHub release. The hardware testing, decisions, and validation were performed by the repository owner. One goal of noting this collaboration is to show other independent and small-scale developers that useful fixes can be researched, validated, documented, and shared publicly without a large development team.

## License

MIT. See [LICENSE](LICENSE).
