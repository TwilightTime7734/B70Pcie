# Tested Results

This document records the exact observed values from the first validated Windows test of the procedure.

## Test platform

GPU:

```text
Intel Arc Pro B70
PCI vendor/device: 8086:E223
```

IGSC:

```text
igsc.exe version 1.3.1.0
igsc library version 1.3.1.0
```

The system was initially reporting the B70 at **PCIe Gen4 x16** despite a Gen5-capable card and platform.

## Initial GFSP read

Command:

```cmd
igsc.exe gfsp generic --cmd 16 --in empty.bin --out config.bin
```

Response length:

```text
20 bytes
```

Raw configuration:

```text
03 00 00 00
03 00 00 00
03 00 00 00
03 00 00 00
00 00 00 00
```

Decoded PCIe Gen4 Downgrade bit 1:

```text
Available    : 1
Current      : 1
Configurable : 1
Pending      : 1
Default      : 0
```

## Write request

The original Pending block was:

```text
03 00 00 00
```

Only bit 1 was cleared, producing:

```text
01 00 00 00
```

Command:

```cmd
igsc.exe gfsp generic --cmd 15 --in downgrade-off.bin --out set-response.bin
```

IGSC reported:

```text
Sending 4 bytes of input data by gfsp generic api
Received 4 bytes of data
Wrote 4 bytes to set-response.bin
```

The command-15 response was:

```text
01 00 00 00
```

## Verification before reboot

A second command-16 read returned:

```text
Available    : 1
Current      : 1
Configurable : 1
Pending      : 0
Default      : 0
```

This showed that firmware had accepted the new pending value while the current boot was still using the old state.

## Verification after cold boot

After a full shutdown and power-on, command 16 returned:

```text
Available    : 1
Current      : 0
Configurable : 1
Pending      : 0
Default      : 0
```

The firmware configuration change was therefore active.

## PCIe result

HWiNFO then confirmed the same Intel Arc Pro B70 operating at:

```text
PCIe 5.0 x16 / 32.0 GT/s
```

This establishes the observed before/after relationship on the test machine:

```text
Downgrade Current = 1  -> PCIe Gen4 x16
Downgrade Current = 0  -> PCIe Gen5 x16
```

This is one validated system, not a claim that every Gen4 B70 problem has the same cause. Users should still verify their own firmware state and platform capabilities before writing anything.
