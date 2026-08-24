# uwdevst_perc

Free Windows x64 percussion synthesizer from the UWdeVST collection.

`uwdevst_perc` is a lightweight synthetic/modal percussion instrument for hybrid percussion, metallic textures, organic impacts and sound-design sketches. It is not a premium sampled percussion library.

## Features

- 9 percussion instruments
- Synthetic and modal sound generation
- Factory presets designed for production and sketching
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

To build a Windows installer from a validated Release build:

```powershell
.\_package_inno.ps1 -Configuration Release -AppVersion 1.0.2
```

Pushing a tag such as `v1.0.2` builds the Standalone and VST3 formats and publishes a Windows x64 package through GitHub Releases.

The public source tree contains only the product targets required for the Standalone and VST3 versions. Internal demo renderers, QA reports and production-test targets are intentionally excluded.

## Repository layout

- `synth-perc/` — plugin and synthesis engine
- `Shared/` — shared runtime code required by this repository
- `assets versions png/` — UI assets required by the build
- `new composants/` — shared UI components required by the build
- `installer/` — Windows installer definition/output staging

## License

The plugin is free to download and use. The source code is **source-available**, not open source. Local inspection, personal modification and personal builds are permitted under [LICENSE.md](LICENSE.md). Redistribution, repackaging and commercial reuse of the source require prior permission.

JUCE is not included in this repository and remains subject to its own licence terms.

Copyright © 2026 Charli Billabert / unicorn who dev.
