# AC6 physical placement validation — 2026-10-05

Anonymous commits prefer physical pages above the 4 KiB E-view addressable
limit, then fall back to the general physical pool. Explicit physical
allocations retain their page-class bounds. This preserves addressable pages
for titles that size a large pool from `MmQueryStatistics`.

Page ownership, decommit/recommit, reader quiescence, and no-zero tests must
probe anonymous-page reuse with anonymous commits, not explicit 4 KiB
allocations. Exact frame-reuse and content assertions remain enforced.

## Validation

- Release `xenon_memory_tests`: passed (1/1, 2.50 seconds).
- Runtime host linked successfully during the full Release build attempt.
- AC6 DLL rebuilt/relinked successfully (recorded exit code 0).
- AC6's pool remained `0x16393000`. The subsequent two `0x80000` allocations
  returned `0xE2F0F000` and `0xE2E8F000`; five `0x10000` allocations returned
  `0xE2E7F000`, `0xE2E6F000`, `0xE2E5F000`, `0xE2E4F000`, and `0xE2E3F000`.
- The observed run still had only one VdSwap and audio callback #1 entered
  without a logged return. Allocation success does not resolve these stalls.
- Full Release build attempts encountered LNK1104 output-file access errors.
  The full CTest run was still pending at the last inspection (124/126 results
  emitted). `xenon_kernel_timer_dispatch_tests` and `xenon_xbox_guest_io_tests`
  were reported Not Run; it is not a validated clean full-suite result.
- The validation host did not exit promptly after `stop.signal`; it was
  subsequently terminated to avoid continued diagnostic-log growth.

Local evidence is under `build/gen10`: `memory-validation.log`,
`full-build.log`, `full-build-retry.log`, `full-validation.log`, and
`ac6-memory-validation/session/capability-report.json`. Runtime diagnostics
are under `build/gen10/launcher`; previous logs were archived before launch.
The launch config requested `logVerbose=false`, but the capability report
still identified verbose diagnostics and the session log grew substantially.

Outstanding: finish full build/test validation, investigate the remaining
video/audio stalls, and commit after validation. No stub path was removed or
replaced as part of this placement/test work.

STATUS: INCOMPLETE