# Manual Procedure

This is the exact Windows procedure used to clear the Intel Arc Pro B70 PCIe Gen4 downgrade flag and restore a Gen5 x16 link on the tested system.

> **Tested hardware:** Intel Arc Pro B70, PCI ID `8086:E223`
>
> **Tested IGSC:** 1.3.1
>
> **Run the IGSC commands from an Administrator Command Prompt.**

## 1. Confirm IGSC and enumerate the device

```cmd
igsc.exe --version
igsc.exe list-devices
```

The tested card enumerated through an Intel GSC/HECI device and reported PCI ID `8086:E223`.

## 2. Read the GFSP configuration

Create an empty input file if one does not already exist:

```cmd
type nul > empty.bin
```

Read GFSP configuration command 16 (`0x10`):

```cmd
igsc.exe gfsp generic --cmd 16 --in empty.bin --out config.bin
```

The successful response was 20 bytes.

Decode PCIe Gen4 Downgrade bit 1 from the five 4-byte fields:

```cmd
powershell -NoProfile -Command "$b=[IO.File]::ReadAllBytes('config.bin'); [pscustomobject]@{Available=(($b[0]-shr 1)-band 1); Current=(($b[4]-shr 1)-band 1); Configurable=(($b[8]-shr 1)-band 1); Pending=(($b[12]-shr 1)-band 1); Default=(($b[16]-shr 1)-band 1)} | Format-List"
```

On the affected card the result was:

```text
Available    : 1
Current      : 1
Configurable : 1
Pending      : 1
Default      : 0
```

Interpretation:

- The setting exists (`Available = 1`).
- It is currently enabled (`Current = 1`).
- Firmware permits changing it (`Configurable = 1`).
- The pending setting is also enabled (`Pending = 1`).
- The firmware default is disabled (`Default = 0`).

## 3. Back up the configuration read

```cmd
copy config.bin config-before-change.bin
```

## 4. Build the 4-byte write request safely

Do **not** hard-code four zero bytes. Intel's implementation preserves the complete existing Pending field and changes only bit 1.

Create a new 4-byte request by copying bytes 12-15 from `config.bin` and clearing only bit 1:

```cmd
powershell -NoProfile -Command "$b=[IO.File]::ReadAllBytes('config.bin'); $w=New-Object byte[] 4; [Array]::Copy($b,12,$w,0,4); $w[0]=$w[0]-band 0xFD; [IO.File]::WriteAllBytes('downgrade-off.bin',$w); Format-Hex .\downgrade-off.bin"
```

On the tested card, the Pending field was:

```text
03 00 00 00
```

Clearing only bit 1 produced:

```text
01 00 00 00
```

The unrelated bit 0 remained set.

## 5. Write the pending configuration

This is the firmware configuration write:

```cmd
igsc.exe gfsp generic --cmd 15 --in downgrade-off.bin --out set-response.bin
```

The successful run reported:

```text
Sending 4 bytes of input data by gfsp generic api
Received 4 bytes of data
Wrote 4 bytes to set-response.bin
```

Inspect the returned bytes:

```cmd
powershell -NoProfile -Command "Format-Hex .\set-response.bin"
```

The tested response was:

```text
01 00 00 00
```

## 6. Verify before rebooting

Do not reboot immediately. Read the full configuration again:

```cmd
igsc.exe gfsp generic --cmd 16 --in empty.bin --out config-after.bin
```

Decode it:

```cmd
powershell -NoProfile -Command "$b=[IO.File]::ReadAllBytes('config-after.bin'); [pscustomobject]@{Available=(($b[0]-shr 1)-band 1); Current=(($b[4]-shr 1)-band 1); Configurable=(($b[8]-shr 1)-band 1); Pending=(($b[12]-shr 1)-band 1); Default=(($b[16]-shr 1)-band 1)} | Format-List"
```

The desired pre-reboot result is:

```text
Available    : 1
Current      : 1
Configurable : 1
Pending      : 0
Default      : 0
```

That means the current boot still uses the old state, but firmware accepted the new state for the next cold boot.

If command 15 fails or the read-back does not show `Pending = 0`, stop and investigate rather than repeatedly issuing writes.

## 7. Cold shutdown and power-on

Use a full shutdown rather than a normal Windows restart:

```cmd
shutdown /s /t 0
```

After the machine is completely off, wait briefly and power it on again.

## 8. Verify after boot

Read GFSP command 16 again:

```cmd
igsc.exe gfsp generic --cmd 16 --in empty.bin --out config-after-reboot.bin
```

Decode:

```cmd
powershell -NoProfile -Command "$b=[IO.File]::ReadAllBytes('config-after-reboot.bin'); [pscustomobject]@{Available=(($b[0]-shr 1)-band 1); Current=(($b[4]-shr 1)-band 1); Configurable=(($b[8]-shr 1)-band 1); Pending=(($b[12]-shr 1)-band 1); Default=(($b[16]-shr 1)-band 1)} | Format-List"
```

The tested card returned:

```text
Available    : 1
Current      : 0
Configurable : 1
Pending      : 0
Default      : 0
```

HWiNFO then confirmed the card had trained at:

```text
PCIe 5.0 x16 / 32.0 GT/s
```

## Recovery consideration

Disabling the forced downgrade allows the GPU/platform to attempt Gen5 link training. If a particular board, slot, riser, or signal path cannot operate reliably at Gen5, force the slot to Gen4 in system BIOS before attempting further firmware changes.
