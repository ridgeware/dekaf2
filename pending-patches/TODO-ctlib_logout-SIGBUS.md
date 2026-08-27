# Pending patch: KSQL::ctlib_logout() — fix SIGBUS + handle leak on dead CT-Lib connections

**For:** Joachim (DEKAF2 maintainer)
**From:** Joe Kuefler
**Raised:** 2026-08-26 (Joachim on PTO this week; merge when back)
**Patch:** `ctlib_logout.patch` in this directory — applies cleanly to
`source/data/sql/ksql.cpp` with `git apply pending-patches/ctlib_logout.patch`
from the repo root (verified against current HEAD).

## TL;DR

`KSQL::ctlib_logout()` has two bugs that only bite when tearing down a SQL Server
(CT-Lib / FreeTDS) connection whose socket has already died. One leaks handles; the
other crashes the process. Both were hit hard by a long-running paxapis warehouse sync
(`pax1`/`pax2`) pulling from a GreatPlains source that sits behind something reaping
long-lived sessions.

This is a genuine DEKAF2 fix, not a paxapis workaround — any dekaf2 caller that closes a
CT-Lib connection after the server has gone away is exposed. paxapis currently only
*reduces how often* it hands dekaf2 a dead connection (see `paxapis/paxsync.{h,cpp}`); it
cannot fix the teardown itself.

## The two defects

**1. Handle leak.** `ct_cancel()` writes an attention packet to the server, so it fails
on a dead socket. The old code did:

```cpp
if (ct_cancel(...) != CS_SUCCEED)
{
    ctlib_api_error("ctlib_logout>ct_cancel");
    return SetError(...);          // <-- returns HERE
}
```

Returning there skips `ct_cmd_drop` / `ct_close` / `ct_con_drop` / `ct_exit` /
`cs_ctx_drop`. `m_pCtCommand`, `m_pCtConnection` and `m_pCtContext` stay allocated and
non-null, but the caller treats the connection as closed and the next `OpenConnection()`
allocates fresh handles. The old set leaks — once per dropped connection, thousands of
times over a multi-hour sync.

**2. SIGBUS (this is the one that took the process down).** `ct_close(conn, CS_UNUSED)`
is a *graceful* close: FreeTDS tries to send a logout token. When the socket is already
gone, that write fails and FreeTDS' own failure path recurses:

```
tds_disconnect -> tds_put_byte -> tds_write_packet
               -> tds_connection_put_packet -> tds_close_socket
               -> tds_disconnect -> ...   (never terminates)
```

It walks off the end of the worker thread stack → SIGBUS at a guard-page address (not a
clean SIGSEGV). Real stack trace from the field was hundreds of these frames deep.

## The fix

- On `ct_cancel` failure, **fall through instead of returning** — drop the command,
  connection and context so nothing leaks — and remember the connection was already dead.
- Close a known-dead connection with **`CS_FORCE_CLOSE`** instead of `CS_UNUSED`, so
  FreeTDS drops the socket without attempting the logout write that triggers the
  recursion. A still-alive connection keeps the graceful `CS_UNUSED` close.
- Return `bOK` (false when we hit the dead-socket path) rather than an unconditional
  `true`, so callers still see the failure.

`CS_FORCE_CLOSE` is standard CT-Lib (`cspublic.h`, value 301); confirmed present in the
FreeTDS we build against (1.5.1).

## Notes for review

- Behavior on a *healthy* close is unchanged: `bWasAlive` stays true, `ct_close` still
  gets `CS_UNUSED`, and the function still returns true.
- Worth a scan for other `ct_close(..., CS_UNUSED)` / early-return-before-cleanup sites in
  the CT-Lib path — this was found via one crash, not an audit.
- If you'd rather express the "connection already dead" signal differently (e.g. reuse an
  existing flag rather than the local `bWasAlive`), the mechanism is easy to swap; the two
  behaviors that matter are (a) always reach the drop/close cleanup and (b) force-close a
  dead socket.

## Repro context

- paxapis `pax2 /etc/patunnel.dbc ProjectA_DashboardBilling` (SQL Server source via ssh
  tunnel → MySQL PAX warehouse), long enough for the source session reaper to fire.
- Before: SIGBUS partway through, or a slow handle leak on shorter runs.
- After (paxapis-side mitigation in place, this patch NOT yet applied): the crash is
  avoided because paxapis now reuses one source connection per worker and drains fast, so
  dekaf2 is rarely handed a dead connection — but when it is, the leak/recursion above is
  still latent in dekaf2. This patch closes it at the source.
