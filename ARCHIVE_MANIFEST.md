# Archive manifest

Archive date: **2026-09-04**  
Archive repository: `unicornwhodev/synthe-preversion-v1-archive`  
Consolidated release tag: `preversion-v1-archive`

The source branch and release tag are recorded separately because each original
`v1.0.2` tag predates its repository's later licence/showcase commit on `main`.
Original commits are retained in Git history; the imported working trees live
under `collections/`.

## Source revisions

| Collection | Original repository | Imported path | `main` commit | Original tag | Tag commit | Archive tag |
| --- | --- | --- | --- | --- | --- | --- |
| Percussion | `unicornwhodev/synth-perc` | `collections/percussion` | `24b4ffcf34e7894b5e1f8a17c7e7e40b29dc02f7` | `v1.0.2` | `743447f98144c92ebefaec226fa0a806ce3e6f82` | `source/perc/v1.0.2` |
| Rare | `unicornwhodev/synthe-instr` | `collections/rare` | `6932f2798ecab4cab3cee44bc6098bbd3490a808` | `v1.0.2` | `a7affb47d4cfad4588edb3fabbe2d0ccb9d48803` | `source/rare/v1.0.2` |
| Orchestral | `unicornwhodev/synthe-orch` | `collections/orchestral` | `3403c6d0952a36cb201cbf7276994b7ae20382c3` | `v1.0.2` | `7cd4b58794cce0f9a59225e87cec2a2a5ecd0831` | `source/orch/v1.0.2` |

All three imported `main` revisions contain `LICENSE.md` at Git blob
`2ac1e766b68bf0ee2aff038b161bcd0f8e22f6bd`.

## Published v1.0.2 assets

| Collection | Asset | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| Percussion | `synth-perc_1.0.2_Windows_x64.zip` | 38,036,126 | `91c7af7da95dc6ed87ac1f5e9b6ff4c773f6d8ee4d4a0ec00ded843a09637c31` |
| Percussion | `UWdeVST_Perc_1.0.2_Linux_x86_64.tar.gz` | 36,116,131 | `2410b266c60129552b1cc2c29cfdf487cab8b584169251150aabf1fa2d2f5c9e` |
| Percussion | `UWdeVST_Perc_1.0.2_macOS_Builder.tar.gz` | 39,667,206 | `92c21387c1e71dbfcc4957b41539b58d5cfabc2b1568c9e86e345501ce0430ed` |
| Rare | `synthe-instr_1.0.2_Windows_x64.zip` | 44,524,146 | `6b98a8645d29ae2ebd779100bed43b182ca7d6d34105ab384d59cf7225baa100` |
| Rare | `UWdeVST_Rare_1.0.2_Linux_x86_64.tar.gz` | 42,701,640 | `4c37c36810a3d829e235e10c00f0e477d280a03c08f205bde701cf70901dc248` |
| Rare | `UWdeVST_Rare_1.0.2_macOS_Builder.tar.gz` | 40,707,520 | `4a6e39995c95b484c196b64a4651ceaec1f27053ed1fa798254f305513d6d946` |
| Orchestral | `synthe-orch_1.0.2_Windows_x64.zip` | 39,694,632 | `9ddb5e07b94ed276d4b642691b700815611a8ef619502286f7bba2d3c1e9a27b` |
| Orchestral | `UWdeVST_Orch_1.0.2_Linux_x86_64.tar.gz` | 37,925,964 | `7964713526636fdbbf5fc1f017809fd1f74cdb5ae505328550232751cf66bed2` |
| Orchestral | `UWdeVST_Orch_1.0.2_macOS_Builder.tar.gz` | 38,250,162 | `d835b5df68cf45a3f01d17fd6bf53929ad539d9d9608386f1803ce2534e96fd3` |

The asset digests were supplied by GitHub and independently checked after
download before publication in the consolidated release.

## Excluded repositories

These repositories were deliberately outside the migration boundary:

| Repository | Baseline `main` commit | Migration action |
| --- | --- | --- |
| `unicornwhodev/synth-piano` | `dc9e07ef4e6e3a4038f8954fe32cc3ed4bf5ef87` | None |
| `unicornwhodev/synth-bass` | `4e0fc4474b5d99960ef3c7a2e1cf419cbb9b2281` | None |
| `unicornwhodev/synth-guitar` | `6b17fb5f65a8c2c112c78bc104bed074297cf1a3` | None |
| `unicornwhodev/synth_drum` | `6d9e5cd986418d1e9581ae5d52b73792a7dbfd04` | None; active editing and out of scope |

