# Soak log template

Copy to `docs/lab/SOAK_<board>_<YYYYMMDD>.md` and fill. Delete unused rows.

## Identity

| Field | Value |
|---|---|
| Date | |
| Operator | |
| Repo commit / tag | |
| IDF version | |
| Board | |
| Blog V4 serial (ok redact) | |
| Antenna / notes | |
| Example / app binary | `examples/p4_serial_smoke` or: |

## Run plan

| Field | Value |
|---|---|
| Duration planned | |
| LO (Hz) | |
| Sample rate (exact SPS) | |
| transfer_bytes × count | |
| Passport run? | yes/no — attach summary |

## Results

| Metric | Start | Mid | End |
|---|---|---|---|
| `effective_sps` | | | |
| `overruns` | | | |
| `consumer_drops` | | | |
| `sample_min` / `max` | | | |
| health `overall` | | | |
| FAULT events | none / describe | | |

## Verdict

| | |
|---|---|
| Pass / fail | |
| Evidence label for PROJECT_TRUTH | Hardware-verified / fail notes |
| Attachments | serial log path, photos optional |

## Notes

(power, hub, ambient RF, anomalies)
