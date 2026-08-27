# pending-patches — for Joachim, when back from PTO

Two independent DEKAF2 bugs found while standing up the paxapis PAX-warehouse sync.
Both come with a ready-to-apply patch (verified `git apply --check` against HEAD),
a write-up, and — for the parser bug — a runnable repro plus its captured output.
Apply from the repo root.

| Bug | Write-up | Patch | Severity |
|-----|----------|-------|----------|
| `KSQL::ctlib_logout()` SIGBUS + handle leak on dead CT-Lib sockets (crashed the multi-hour `pax1`/`pax2` syncs) | `TODO-ctlib_logout-SIGBUS.md` | `git apply pending-patches/ctlib_logout.patch` | crash / leak |
| `kParseTimestamp()` rejects hour 12 with an AM/PM meridiem (midnight & noon → Invalid) | `TODO-kParseTimestamp-hour12.md` | `git apply pending-patches/kparsetimestamp_hour12.patch` | correctness |

The kParseTimestamp bug ships with `kparsetimestamp_hour12_repro.cpp` and its verbatim
run output `kparsetimestamp_hour12.evidence.txt` — 22 of 24 hour/meridiem rows parse,
only hour 12 fails, using dekaf2's own documented 12-hour format string.

These are genuine DEKAF2 fixes, not paxapis workarounds; paxapis only mitigates around
them for now (see `paxapis/paxsync.cpp`).
