# Windows release packages

The normal download is the native `B70Pcie.exe` package. It contains the required
IGSC and MeTee functionality and requires no separate DLLs.

After building and testing, run:

```powershell
./release/package.ps1
```

This creates `build/release/B70Pcie-v1.1-win64.zip` and its `.sha256` file. To
package a specific previously validated executable, pass `-BinaryPath` with its
path. Packaging does not rebuild or modify the executable.

The ZIP contains the executable, Windows usage instructions, the MIT license,
third-party notices, Intel IGSC and MeTee licenses, the MeTee Windows WDK notice,
and SHA-256 checksums for the packaged files.

See [v1.1 release notes](RELEASE_NOTES_v1.1.md) for the changes and test scope.
Publish a release from the corresponding source commit and upload the ZIP and
its checksum after reviewing the source update.

The previous [manual IGSC package](https://github.com/TwilightTime7734/B70Pcie/releases/tag/v0.1-manual)
remains available. Its `igsc.exe` and `igsc.dll` must remain side-by-side. The
manual binaries were locally built from Intel's upstream IGSC 1.3.1 source and
are not presented as official Intel-distributed binaries.
