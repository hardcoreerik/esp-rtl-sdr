# 0.7.2 runtime hardening (review response)

External review (ChatGPT) of the 0.7.1-era tree flagged that architecture and
clean-room discipline are strong, but **concurrency / lifecycle / teardown**
were the real production risks — not more RF reverse engineering.

This release pauses Phase 3 gain/bias **hardware** work for one hardening cut.

## P0 addressed in 0.7.2

| Issue | Fix |
|---|---|
| Concurrent `start()` race (unlock during EP0) | `STARTING` state; second start → `BUSY` |
| Teardown vs 50 ms delay UAF | Worker tasks `xTaskNotifyGive` join waiter; uninstall waits |
| `emit` under lock / reentrancy depth miss | Atomic `in_callback_depth`; `select_device*` defers events until unlock |
| Partial `ensure_ring` failure | Transactional allocate + `destroy_iq_ring` rollback |
| `struct_size` exact match breaks ABI growth | Accept min..sizeof; default trailing fields |
| Callback `retune` returns OK without apply | Strict `ERR_REENTRANT` from callback |
| Kconfig unused | Wired into `config_default` when `CONFIG_*` present |
| Byte-loop pull ring | Block `memcpy` (≤2 copies per push) |
| Component targets | `idf_component.yml` `targets: [esp32p4]` |

## Still open (honest)

| Item | Status |
|---|---|
| True async retune from callback (queue + owner drain) | **Done in 0.7.3** |
| Delivery modes CALLBACK/READ/BOTH | Planned |
| Lazy pull-ring (optional sync read) | Planned |
| ESP-IDF P4 compile CI | **Done** in CI (`idf-p4-build`, IDF 5.3.2 + 5.4.1) |
| Full atomics ownership model for all USB fields | Incremental |
| Hardware soak from this tree | Lab |
| Phase 3 gain/bias EP0 | Lab capture |

## Review agreement

We agree: **0.7.x is promising, not production-ready**.  
Target: *boring to crash*, then expand radio features.
