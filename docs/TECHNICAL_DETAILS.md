# Technical Details

## Firmware path

Intel Compute Runtime's Linux Sysman implementation changes the PCIe link-speed downgrade setting through Intel GSC firmware rather than by directly writing PCIe configuration space.

The relevant sequence is:

1. GFSP command `0x10` / decimal `16` — read configuration.
2. Copy the existing 4-byte Pending field.
3. Modify PCIe downgrade bit 1 only.
4. GFSP command `0x0F` / decimal `15` — write the updated 4-byte request.
5. Read command 16 again and verify the pending value.
6. Cold boot when Pending differs from Current.

## Command 16 response layout

The observed response is 20 bytes consisting of five little-endian 4-byte bitmaps:

| Offset | Length | Meaning |
|---:|---:|---|
| 0 | 4 | Available |
| 4 | 4 | Current |
| 8 | 4 | Configurable |
| 12 | 4 | Pending |
| 16 | 4 | Default |

Intel's upstream constants use those same offsets.

PCIe Gen4 Downgrade is **bit 1** in each bitmap.

For example, the affected test card returned:

```text
03 00 00 00  Available
03 00 00 00  Current
03 00 00 00  Configurable
03 00 00 00  Pending
00 00 00 00  Default
```

For bit 1 this decodes to:

```text
Available    = 1
Current      = 1
Configurable = 1
Pending      = 1
Default      = 0
```

## Why the write request was `01 00 00 00`

The Pending field was:

```text
03 00 00 00
```

The first byte is binary:

```text
03 = 00000011
```

Clearing only bit 1 produces:

```text
01 = 00000001
```

Therefore the request became:

```text
01 00 00 00
```

This is materially safer than sending `00 00 00 00`, because bit 0 and all other unrelated settings are preserved.

Equivalent logic:

```c
uint8_t request[4];
memcpy(request, response + 12, 4);
request[0] &= ~(1u << 1); // disable PCIe Gen4 downgrade
```

To enable the downgrade setting instead:

```c
request[0] |= (1u << 1);
```

## Public IGSC API

Intel IGSC exposes a public generic GFSP function:

```c
int igsc_gfsp_heci_cmd(
    struct igsc_device_handle *handle,
    uint32_t gfsp_cmd,
    uint8_t *in_buffer,
    size_t in_buffer_size,
    uint8_t *out_buffer,
    size_t out_buffer_size,
    size_t *actual_out_buffer_size
);
```

This lets a purpose-built Windows utility implement the same read/modify/write operation directly without shelling out to the `igsc.exe` command-line program.

## Device enumeration

IGSC also exposes device iteration and device information structures including:

- Windows device path
- PCI domain/bus/device/function
- vendor ID
- device ID
- subsystem IDs

For the first public version of `B70Pcie.exe`, write operations should be restricted to the device ID actually tested by this project:

```text
Vendor: 8086
Device: E223
```

Read-only status may be useful more broadly, but untested devices should not receive command 15 writes simply because they enumerate through IGSC.

## Planned utility safeguards

A safe write path should:

1. Tell the user to **save all work and close running applications before continuing**.
2. Require Administrator privileges / successful IGSC device access.
3. Enumerate devices and display PCI BDF and IDs.
4. Refuse writes to untested device IDs by default.
5. Read command 16 and require at least 20 response bytes.
6. Require `Available = 1` and `Configurable = 1` for bit 1.
7. Copy exactly bytes 12-15 into the request.
8. Change only bit 1.
9. If Pending already equals the requested state, make no write.
10. Send command 15 only once.
11. Require a 4-byte response.
12. Read command 16 again and verify the requested Pending value.
13. Clearly state when a cold shutdown/power-on is required.

## Upstream source references

Intel IGSC:

- `include/igsc_lib.h.in`
- `src/igsc_cli.c`

Intel Compute Runtime:

- `level_zero/sysman/source/shared/firmware_util/sysman_firmware_util.h`
- `level_zero/sysman/source/api/pci/linux/sysman_os_pci_imp.cpp`

Those projects use permissive open-source licenses; this project does not copy Intel implementation source into the utility and instead calls the published IGSC API.
