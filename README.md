<!-- UWDEVST-SHOWCASE:START -->
<p align="center">
  <img src="docs/social-preview.jpg" width="960" alt="UWdeVST Orchestra — UWdeVST collection artwork" />
</p>

<h1 align="center">UWdeVST Orchestra</h1>

<p align="center"><strong>Think bigger.</strong><br />A synthetic orchestral palette for sketching arrangements and exploring hybrid compositions.</p>

<p align="center">
  <a href="https://unicorsoundengine.com/en/plugins/synthe-orch#listen">Listen</a> ·
  <a href="https://unicorsoundengine.com/en/plugins/synthe-orch#install">Download</a> ·
  <a href="https://unicorsoundengine.com/en">Full collection</a> ·
  <a href="https://github.com/unicornwhodev/synthe-orch/issues/new/choose">Report an issue</a>
</p>

**Windows x64 · VST3 · Standalone**

- 20 orchestral instruments
- Strings, woodwinds, brass and percussion
- Algorithmic sound generation

> **Publicly viewable source — proprietary license.** Official binaries are free for individuals and organizations with no more than EUR 100,000 in worldwide consolidated gross revenue. Modification and redistribution are not permitted. Professional use above that threshold requires a paid written license. [Read the license](https://unicorsoundengine.com/en/license) or [request a commercial license](https://unicorsoundengine.com/en/contact).

The license included with each tagged release governs that release. The v1.0 license applies prospectively and does not withdraw permissions already granted on earlier releases.
<!-- UWDEVST-SHOWCASE:END -->

---

# uwdevst_orch

Free Windows x64 algorithmic orchestral synthesizer from the UWdeVST collection.

`uwdevst_orch` is a lightweight orchestral instrument for sketching, education, hybrid scoring and arrangement prototyping. It is not a replacement for a premium multi-sampled orchestral library.

## Features

- 20 orchestral instruments
- 4 families: strings, woodwinds, brass and percussion
- Algorithmic/synthetic sound generation
- Factory presets for orchestral sketching and lightweight production
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

To build the Windows installer from a validated Release build:

```powershell
.\_package_inno.ps1 -Configuration Release -AppVersion 1.0.2
```

Pushing a tag such as `v1.0.2` builds the Standalone and VST3 formats and publishes a Windows x64 package through GitHub Releases.

The public source tree contains only the product targets required for the Standalone and VST3 versions. Internal renderers, QA reports, development plans and production-test targets are intentionally excluded.

## Repository layout

- `synthe-orch/` — plugin and orchestral synthesis engine
- `Shared/` — shared runtime code required by this repository
- `assets_versions_png/` — UI assets required by the build
- `installer/` — Windows installer definition/output staging

## License

Official binaries are free for qualifying users. The source code is publicly viewable under a proprietary license. Viewing and private compilation of strictly unchanged source are permitted; modification, redistribution, repackaging and reuse are not. See [LICENSE.md](LICENSE.md).

JUCE is not included in this repository and remains subject to its own licence terms.

Copyright © 2026 Charli Billabert / unicorn who dev.
