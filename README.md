# uwdevst_rare

Free Windows x64 rare/world instrument synthesizer from the UWdeVST collection.

`uwdevst_rare` is a lightweight synthetic instrument focused on unusual acoustic colours, world-inspired timbres, drones and experimental textures. It is not a sampled ethnomusicology library or a replacement for specialist sample collections.

## Features

- 21 rare/world instruments
- 4 instrument families
- Factory `Reference` and `Signature` presets
- Standalone application and VST3 plugin
- Windows x64
- JUCE 8.0.4 / CMake project

## Download

Ready-to-use builds are distributed through the repository **Releases** page. Use the published installer/package for normal installation.

## Build from source

Requirements: Windows x64, CMake 3.22+, Visual Studio 2022 with the C++ desktop workload, Git and PowerShell.

```powershell
.\_build_all.ps1 -Configuration Release -BootstrapJuce
```

Or use an existing JUCE 8.0.4 checkout:

```powershell
.\_build_all.ps1 -Configuration Release -JuceDir C:\Dev\JUCE
```

To build the Windows installer:

```powershell
.\_build_installer.ps1 -BuildFirst -AppVersion 1.0.2
```

Pushing a tag such as `v1.0.2` builds the Standalone and VST3 formats and publishes a Windows x64 package through GitHub Releases.

The public source tree contains only the product targets required for the Standalone and VST3 versions. Internal renderers, QA reports, development plans and production-test targets are intentionally excluded.

## Repository layout

- `synthe-instr/` — plugin and synthesis engine
- `Shared/` — shared runtime code required by this repository
- `assets versions png/` — UI assets required by the build
- `new composants/` — shared UI components required by the build
- `UWdeVST_Rare_InnoSetup.iss` — Windows installer definition

## License

The plugin is free to download and use. The source code is **source-available**, not open source. Local inspection, personal modification and personal builds are permitted under [LICENSE.md](LICENSE.md). Redistribution, repackaging and commercial reuse of the source require prior permission.

JUCE is not included in this repository and remains subject to its own licence terms.

Copyright © 2026 Charli Billabert / unicorn who dev.
