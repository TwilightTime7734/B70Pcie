# Third-Party Notices

B70Pcie incorporates source from Intel open-source projects. The B70Pcie project itself is licensed under the repository's MIT License. Third-party components retain their own licenses and copyright notices.

## Intel IGSC

Project: Intel Graphics System Firmware Update Library (IGSC FUL)

Upstream repository: https://github.com/intel/igsc

Pinned version/commit used by B70Pcie:

- Version: 1.3.1
- Commit: `483f0b91d4636cbdc8f0503343f858ff2da45432`

IGSC is licensed under the Apache License 2.0. The complete upstream license is available in the pinned `third_party/igsc` submodule as `LICENSE.txt`.

## Intel MeTee

Project: Intel MeTee

Upstream repository: https://github.com/intel/metee

Pinned version/commit used by B70Pcie:

- Version: 6.2.5
- Commit: `63445aff21f8051926f69d8ac53df926ffc09372`

MeTee is licensed under the Apache License 2.0. The complete upstream license is available in the pinned `third_party/metee` submodule as `COPYING`. Additional source-specific notices, including the Windows WDK license notice used by the upstream project, remain present in the pinned submodule.

## Build model

`build.ps1` compiles the required IGSC and MeTee source directly into `B70Pcie.exe`. The release executable therefore does not require separate `igsc.dll` or MeTee DLL files.

B70Pcie is an independent community project and is not affiliated with or endorsed by Intel.
