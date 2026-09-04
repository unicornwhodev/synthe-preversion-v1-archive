# Synthé préversion v1 — archive

> Archive publique et figée des collections UWdeVST antérieures à la prochaine génération.

This repository preserves the pre-v1 source history and published `v1.0.2`
packages for three UWdeVST collections. It is an archive, not the development
home of the new collections currently in production.

## Archived collections

| Collection | Archived source | Original repository | Imported `main` |
| --- | --- | --- | --- |
| Percussion | [`collections/percussion`](collections/percussion) | [`synth-perc`](https://github.com/unicornwhodev/synth-perc) | `24b4ffcf34e7894b5e1f8a17c7e7e40b29dc02f7` |
| Rare | [`collections/rare`](collections/rare) | [`synthe-instr`](https://github.com/unicornwhodev/synthe-instr) | `6932f2798ecab4cab3cee44bc6098bbd3490a808` |
| Orchestral | [`collections/orchestral`](collections/orchestral) | [`synthe-orch`](https://github.com/unicornwhodev/synthe-orch) | `3403c6d0952a36cb201cbf7276994b7ae20382c3` |

Each collection remains self-contained. Shared code, build scripts, artwork,
documentation and the embedded proprietary public-source license were retained
without consolidation or refactoring.

The exact original release commits are also preserved under these namespaced
Git tags:

- `source/perc/v1.0.2`
- `source/rare/v1.0.2`
- `source/orch/v1.0.2`

The nine original Windows, Linux and macOS-builder release assets are gathered
in the [`preversion-v1-archive` release](../../releases/tag/preversion-v1-archive).
The macOS files are source/build packages; they are not compiled, signed or
notarized macOS binaries.

See [`ARCHIVE_MANIFEST.md`](ARCHIVE_MANIFEST.md) for immutable source and release
provenance, [`SHA256SUMS.txt`](SHA256SUMS.txt) for package checksums, and
[`MIGRATION_RECEIPT.md`](MIGRATION_RECEIPT.md) for the completed GitHub migration
checks.

## Explicitly outside this archive

The following repositories remain independent and were not imported, changed
or archived:

- [`synth-piano`](https://github.com/unicornwhodev/synth-piano)
- [`synth-bass`](https://github.com/unicornwhodev/synth-bass)
- [`synth-guitar`](https://github.com/unicornwhodev/synth-guitar)
- [`synth_drum`](https://github.com/unicornwhodev/synth_drum) — active editing; outside this archive's scope

## License

This consolidation does not change any product license. Every collection keeps
the `LICENSE.md` file present at its imported `main` commit. The license included
with a tagged release governs that release.
