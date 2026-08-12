# esp_rtl_sdr — Documentation Standard

> Same discipline as [TheOrc `DOCUMENTATION_STANDARD`](https://github.com/hardcoreerik/TheOrc/blob/master/docs/DOCUMENTATION_STANDARD.md),
> adapted for a driver component.

---

## Accuracy first

Every claim must reflect current implementation or be marked planned.

**Do not document planned behavior as current behavior.**

| Marking | Meaning |
|---|---|
| **Implemented** / Done | In source in this repo |
| **Provenance** | Measured under another project (e.g. OrcSDR); re-soak from this tree open |
| **Hardware-verified** | Observed on named hardware **from a build of this tree** with a log reference |
| **Formula** | Math/EP0 path exists; continuous host soak not claimed |
| **Planned** | Roadmap only |
| **Deferred** | Intentionally not now |

Authoritative current state: **[PROJECT_TRUTH.md](../PROJECT_TRUTH.md)**.  
When docs disagree, **PROJECT_TRUTH wins**.

---

## File naming

| Kind | Convention | Examples |
|---|---|---|
| Root truth / plan | `UPPER_SNAKE` or established names | `PROJECT_TRUTH.md`, `CHANGELOG.md` |
| Docs suite | `UPPER_SNAKE.md` preferred for new files | `VISION.md`, `SILICON.md`, `TESTING.md` |
| Legacy | Keep links working | `Project_truth.md` → stub to `PROJECT_TRUTH.md` if present |

---

## Doc structure

```markdown
# esp_rtl_sdr — <Title>

> Optional callout: status, disclaimer, phase flag.

---

## Section

Content...
```

- Prefer tables for comparisons and status matrices.
- Link to authoritative files instead of duplicating claims.
- No empty placeholder docs.

---

## Implementation vs vision

- `VISION.md` may describe the **nervous system** north star.
- Only enable capability bits (`CAP_*`) for paths that exist in code.
- Roadmap items stay unchecked until source lands; then update PROJECT_TRUTH
  in the **same** change set when possible.

---

## Clean-room and third-party docs

When citing RTL2832U / R820T2 / Blog V4 material:

1. Say whether the source is **public product DS**, **public register PDF**,
   **kernel headers**, **community leak (NDA-era DS)**, or **our USB capture**.
2. Never imply librtlsdr source was used as implementation input.
3. Do not paste large register dumps from NDA-era material into the tree;
   summarize and keep measured EP0 in profile headers.

---

## Hardware and lab claims

Before writing “works on …”:

1. Name the **host**, **dongle profile**, and **evidence label**.
2. Lab gear (Heltec, Baofeng, Flipper) is for **stimulus / observation** —
   disclose legal TX limits; do not claim they replace USB capture for gain.
3. Companion-app results (OrcSDR) are **Provenance**, not Hardware-verified for
   this repo, until re-run from `examples/` or a consumer of this tree.

---

## Version alignment

Bump together when releasing:

- `ESP_RTL_SDR_VERSION_*` in `include/esp_rtl_sdr.h`
- `idf_component.yml` / `library.json`
- `CHANGELOG.md`
- `PROJECT_TRUTH.md` snapshot date and version line
- README version badge

---

## Updating docs

When a feature changes:

1. Prefer docs in the **same commit** as the code.
2. Move Planned → Implemented only when source lands.
3. Move Implemented → Hardware-verified only with a named log / run note.
4. If a claim was oversold, **retract in CHANGELOG** rather than soft-edit quietly
   (TheOrc-style honesty).

---

## What not to include

- Personal absolute paths as the only install method (give relative / example paths)
- Credentials, private serials, or identifiable lab network details
- Empty “TODO doc” files
- Marketing language that contradicts PROJECT_TRUTH
