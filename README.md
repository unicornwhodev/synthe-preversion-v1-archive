<!-- UWDEVST-SHOWCASE:START -->
<p align="center">
  <img src="docs/social-preview.jpg" width="960" alt="UWdeVST Rare — UWdeVST collection artwork" />
</p>

<h1 align="center">UWdeVST Rare</h1>

<p align="center"><strong>Other sonic horizons.</strong><br />Rare timbres and colors inspired by instruments from around the world for melodies and textures.</p>

<p align="center">
  <a href="https://unicorsoundengine.com/en/plugins/synthe-instr#listen">Listen</a> ·
  <a href="https://unicorsoundengine.com/en/plugins/synthe-instr#install">Download</a> ·
  <a href="https://unicorsoundengine.com/en">Full collection</a> ·
  <a href="https://github.com/unicornwhodev/synthe-instr/issues/new/choose">Report an issue</a>
</p>

**Windows x64 · VST3 · Standalone**

- 21 rare and world-inspired instruments
- 4 instrument families
- Reference and Signature presets

> **Publicly viewable source — proprietary license.** Official binaries are free for individuals and organizations with no more than EUR 100,000 in worldwide consolidated gross revenue. Modification and redistribution are not permitted. Professional use above that threshold requires a paid written license. [Read the license](https://unicorsoundengine.com/en/license) or [request a commercial license](https://unicorsoundengine.com/en/contact).

The license included with each tagged release governs that release. The v1.0 license applies prospectively and does not withdraw permissions already granted on earlier releases.
<!-- UWDEVST-SHOWCASE:END -->

---

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

Official binaries are free for qualifying users. The source code is publicly viewable under a proprietary license. Viewing and private compilation of strictly unchanged source are permitted; modification, redistribution, repackaging and reuse are not. See [LICENSE.md](LICENSE.md).

JUCE is not included in this repository and remains subject to its own licence terms.

Copyright © 2026 Charli Billabert / unicorn who dev.
