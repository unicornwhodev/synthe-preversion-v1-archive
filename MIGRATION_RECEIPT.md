# Migration receipt

Result: **PASS**  
Completed: **2026-09-04**

## Consolidated destination

- Public repository: `unicornwhodev/synthe-preversion-v1-archive`
- Consolidated release: `preversion-v1-archive`
- Release commit: `90afa08eaecce05fe00fa56776c3d7b11bf83400`
- Release state: final, not draft, not prerelease
- Published assets: 9
- Published asset bytes: 357,623,527
- Every remote asset size and GitHub SHA-256 digest matched the original release
  and the independently downloaded verification copy.

Imported source-tree identity checks:

| Path | Git tree | Result |
| --- | --- | --- |
| `collections/percussion` | `f937ba0318bb4b27a87a9280238b1cfa9122c038` | Exact match |
| `collections/rare` | `098f7459d57d92c6c9920dfab4a27c1aefbfde72` | Exact match |
| `collections/orchestral` | `f2af6eb0968c9b4a95a4ad1eadc7f6eac9554e50` | Exact match |

The namespaced source tags resolve to the original release commits recorded in
`ARCHIVE_MANIFEST.md`.

## Original repositories after migration

| Repository | `main` unchanged | GitHub archive state | Release assets retained |
| --- | --- | --- | --- |
| `unicornwhodev/synth-perc` | `24b4ffcf34e7894b5e1f8a17c7e7e40b29dc02f7` | Archived | 3 |
| `unicornwhodev/synthe-instr` | `6932f2798ecab4cab3cee44bc6098bbd3490a808` | Archived | 3 |
| `unicornwhodev/synthe-orch` | `3403c6d0952a36cb201cbf7276994b7ae20382c3` | Archived | 3 |

No original repository was deleted.

## Exclusion boundary after migration

| Repository | `main` unchanged | GitHub archive state | Action taken |
| --- | --- | --- | --- |
| `unicornwhodev/synth-piano` | `dc9e07ef4e6e3a4038f8954fe32cc3ed4bf5ef87` | Active | None |
| `unicornwhodev/synth-bass` | `4e0fc4474b5d99960ef3c7a2e1cf419cbb9b2281` | Active | None |
| `unicornwhodev/synth-guitar` | `6b17fb5f65a8c2c112c78bc104bed074297cf1a3` | Active | None |
| `unicornwhodev/synth_drum` | `6d9e5cd986418d1e9581ae5d52b73792a7dbfd04` | Active | None |

The existing local working copies of every source repository were left
untouched. All imports came from freshly fetched GitHub references.

