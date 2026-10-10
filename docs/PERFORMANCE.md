# Performance

Do **not** claim MyDesk is faster than Autodesk products without matched hardware, datasets, and methodology.

## Targets (budgets, not guarantees)

| Scenario | Budget | Status |
|---|---|---|
| 2D drafting input→visible p95 (small/medium dwg) | < 50 ms | not measured yet |
| Viewport FPS midrange hardware | 60 FPS target | not measured yet |
| Open/save `.mydesk` 10k entities | regression dashboard | no baseline yet |
| Undo latency | < 16 ms typical small tx | not measured yet |

## Measured (this workspace)

| Date | Machine | Scenario | Result |
|---|---|---|---|
| 2026-10-10 | Apple Silicon (dev) | `mydesk_tests` suite | functional correctness only; no perf harness yet |

## Methodology (when measuring)

1. Document CPU/GPU/RAM/OS and MyDesk commit SHA.
2. Use fixed fixtures under `benchmarks/` (to be populated).
3. Warm-up + N runs; report median and p95.
4. Check regression thresholds into CI only after baselines exist.
