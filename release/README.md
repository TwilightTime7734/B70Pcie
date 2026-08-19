# Manual IGSC package

This folder contains the current Windows manual-use package for the validated Intel Arc Pro B70 PCIe Gen4 Downgrade procedure.

## Package

`B70Pcie-Manual-IGSC-1.3.1-win64.zip`

Inside the ZIP, `igsc.exe` and its matching `igsc.dll` are kept **side-by-side in the same directory**. Do not separate them. The package also contains Intel's Apache-2.0 license, SHA-256 checksums, and a short README.

These IGSC binaries were locally built from Intel's upstream open-source IGSC 1.3.1 source and were the binaries used to validate the procedure documented in this repository. They are not presented as official Intel-distributed binaries.

The final `B70Pcie.exe` release will replace the raw manual GFSP workflow for normal users.
