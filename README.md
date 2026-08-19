# B70Pcie

Windows tooling and documentation for inspecting and changing the Intel Arc Pro B70 **PCIe Gen4 Downgrade** firmware configuration.

## Download for Windows

**[⬇ Download the B70Pcie Manual IGSC Package for Windows x64](https://github.com/TwilightTime7734/B70Pcie/releases/download/v0.1-manual/B70Pcie-Manual-IGSC-1.3.1-win64.zip)**

Unzip the package and keep **`igsc.exe` and `igsc.dll` together in the same folder**. Run the manual procedure from an **Administrator Command Prompt**.

This is the current manual package. A guarded `B70Pcie.exe` is planned to automate the process.

See: [Manual procedure](docs/MANUAL_PROCEDURE.md)

## Why this exists

A tested Intel Arc Pro B70 was limited to **PCIe Gen4 x16** even though the card and platform were Gen5-capable. Reading the GPU's GSC/GFSP configuration directly showed the PCIe Gen4 downgrade setting enabled.

After clearing only that configuration bit, verifying the pending state, and performing a cold shutdown/power-on, the same card reported:

```text
PCIe Gen4 Downgrade: disabled
PCIe link:            Gen5 x16 / 32.0 GT/s
```

HWiNFO confirmed the restored **PCIe 5.0 x16** link.

## Project status

The low-level procedure has been validated on an Intel Arc Pro B70 with PCI device ID **8086:E223** on Windows using Intel IGSC 1.3.1.

The next project milestone is a guarded Windows executable (`B70Pcie.exe`) that will automate the validated sequence without requiring users to create binary files or issue raw GFSP commands.

Planned interface:

```text
B70Pcie.exe list
B70Pcie.exe status
B70Pcie.exe disable
B70Pcie.exe enable
```

The write path will be intentionally conservative: read first, validate the response, preserve every unrelated configuration bit, change only the PCIe downgrade bit, write once, and read back to verify the pending state.

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

## Important safety note

This changes a persistent GPU firmware **configuration value**. It is not a firmware image flash, but it is still a firmware-level write.

The manual procedure should currently be treated as **tested on the Arc Pro B70 8086:E223 only**. Do not blindly send command 15 with a hard-coded value. Always read the existing Pending field first and preserve every bit except bit 1.

If Gen5 link training is unstable on a particular platform, forcing the slot back to Gen4 in system BIOS is a practical recovery path.

## Requirements for the manual method

- Windows
- Administrator Command Prompt
- Intel graphics driver exposing the GSC/HECI device
- Intel IGSC with the generic GFSP command enabled

The procedure was validated with **IGSC 1.3.1**.

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

## License

MIT. See [LICENSE](LICENSE).
