# Pending patch: kParseTimestamp() — hour 12 with an AM/PM meridiem is wrongly rejected

**For:** Joachim (DEKAF2 maintainer)
**From:** Joe Kuefler
**Raised:** 2026-08-27 (Joachim on PTO this week; merge when back)
**Baseline:** dekaf2 `master` @ `573ed648`
**Patch:** `kparsetimestamp_hour12.patch` in this directory — applies cleanly to
`source/time/clock/ktime.cpp` with
`git apply pending-patches/kparsetimestamp_hour12.patch` from the repo root
(verified against current HEAD).
**Repro:** `kparsetimestamp_hour12_repro.cpp` (self-contained, one build line below).
**Captured output:** `kparsetimestamp_hour12.evidence.txt`.

## TL;DR

`kParseTimestamp()` cannot parse a 12-hour timestamp whose hour is **12** — i.e.
midnight and noon. Every other hour (1–11, AM and PM) parses. It is not a bad format
string: it fails with dekaf2's **own** documented 12-hour format, and the exact same
format parses all 22 other hour/meridiem combinations in the same run.

    "Jun 15, 2024 12:00:00 AM"  ->  Invalid   (should be 2024-06-15 00:00:00)
    "Jun 15, 2024 12:00:00 PM"  ->  Invalid   (should be 2024-06-15 12:00:00)

Yes — Herr Wecker, I know you'll want proof before you touch `ktime.cpp`. Read on. 🙂

## Irrefutable evidence

One run, **one format string for every row** —
`"NNN DD, YYYY hh:mm:ss aa"`, copied verbatim from dekaf2's own format table in
`ktime.cpp` (~line 1056: `// Dec 02, 2017 2:39:58 AM`). 24 rows in, 22 out, and the
only two failures in the whole matrix are hour 12:

```
Same format string for every row: "NNN DD, YYYY hh:mm:ss aa"

  input                         24h-clock  kParseTimestamp result
  -----                         ---------  ----------------------
  Jun 15, 2024 01:00:00 AM      01:00:00   2024-06-15 01:00:00
  Jun 15, 2024 01:00:00 PM      13:00:00   2024-06-15 13:00:00
  Jun 15, 2024 02:00:00 AM      02:00:00   2024-06-15 02:00:00
  Jun 15, 2024 02:00:00 PM      14:00:00   2024-06-15 14:00:00
  Jun 15, 2024 03:00:00 AM      03:00:00   2024-06-15 03:00:00
  Jun 15, 2024 03:00:00 PM      15:00:00   2024-06-15 15:00:00
  Jun 15, 2024 04:00:00 AM      04:00:00   2024-06-15 04:00:00
  Jun 15, 2024 04:00:00 PM      16:00:00   2024-06-15 16:00:00
  Jun 15, 2024 05:00:00 AM      05:00:00   2024-06-15 05:00:00
  Jun 15, 2024 05:00:00 PM      17:00:00   2024-06-15 17:00:00
  Jun 15, 2024 06:00:00 AM      06:00:00   2024-06-15 06:00:00
  Jun 15, 2024 06:00:00 PM      18:00:00   2024-06-15 18:00:00
  Jun 15, 2024 07:00:00 AM      07:00:00   2024-06-15 07:00:00
  Jun 15, 2024 07:00:00 PM      19:00:00   2024-06-15 19:00:00
  Jun 15, 2024 08:00:00 AM      08:00:00   2024-06-15 08:00:00
  Jun 15, 2024 08:00:00 PM      20:00:00   2024-06-15 20:00:00
  Jun 15, 2024 09:00:00 AM      09:00:00   2024-06-15 09:00:00
  Jun 15, 2024 09:00:00 PM      21:00:00   2024-06-15 21:00:00
  Jun 15, 2024 10:00:00 AM      10:00:00   2024-06-15 10:00:00
  Jun 15, 2024 10:00:00 PM      22:00:00   2024-06-15 22:00:00
  Jun 15, 2024 11:00:00 AM      11:00:00   2024-06-15 11:00:00
  Jun 15, 2024 11:00:00 PM      23:00:00   2024-06-15 23:00:00
  Jun 15, 2024 12:00:00 AM      00:00:00   **FAIL (Invalid)**
  Jun 15, 2024 12:00:00 PM      12:00:00   **FAIL (Invalid)**

Control group (proves the harness itself is fine):
  ISO auto-detect     '2024-06-15 12:00:00' -> 2024-06-15 12:00:00
  11 AM (hour 1..11)  same format           -> 2024-06-15 11:00:00
  12 AM (the defect)  same format           -> **FAIL (Invalid)**
```

The control group at the bottom shuts every door:

- **ISO `12:00:00` parses** → the harness and `kFormTimestamp` are fine; 12 isn't a
  formatting problem.
- **`11:00 AM` parses with the same format** → the format string is fine.
- **`12:00 AM` fails with that same format** → the defect is specific to hour 12 + meridiem.

So the failure is neither "wrong format" nor "bad test harness." It is the parser.

## Root cause — `source/time/clock/ktime.cpp`, the `case 'a'` block (~L787–806)

```cpp
case 'a':
    if (tm.hour > 11) return Invalid;   // 12 AM rejected — but 12 AM is 00:00
    break;
...
case 'p':
    if (tm.hour > 11) return Invalid;   // 12 PM rejected — but 12 PM is 12:00
    tm.hour += 12;                       // and 1..11 PM correctly become 13..23
    break;
```

The logic has the 12-hour clock backwards. On a 12-hour dial the hours run **1..12**,
and **12 is the special case**, not an out-of-range value:

- **12 AM = 00:00** (midnight) — must map hour 12 → 0.
- **12 PM = 12:00** (noon) — must keep hour 12 as-is (no +12).
- 1..11 AM stay; 1..11 PM add 12.

Guarding with `hour > 11` treats 12 as invalid in both branches, which is exactly the
one value a 12-hour parser has to accept and remap.

## The fix (`kparsetimestamp_hour12.patch`)

```diff
@@ -789,16 +789,17 @@ detail::KParsedTimestamp::raw_time detail::KParsedTimestamp::Parse(KStringView s
 
 				switch (ch)
 				{
-					case 'a':
-						if (tm.hour > 11) return Invalid;
+					case 'a': // AM: 12-hour clock runs 1..12; 12 AM is midnight (00)
+						if (tm.hour > 12) return Invalid;
+						if (tm.hour == 12) tm.hour = 0;
 						break;
 
 					case 'm':
 						break;
 
-					case 'p':
-						if (tm.hour > 11) return Invalid;
-						tm.hour += 12;
+					case 'p': // PM: 1..11 map to +12; 12 PM is noon and stays 12
+						if (tm.hour > 12) return Invalid;
+						if (tm.hour != 12) tm.hour += 12;
 						break;
 
 					default:
```

Smallest safe change: widen the guard `> 11` → `> 12` so 12 is accepted, then special-case
it (AM 12→0, PM 12 stays). Hours 1–11 are untouched, so no regression on the 22 rows that
already pass; hour 0 (if a caller ever feeds "00 ... AM/PM") behaves exactly as before.

## Reproduce it yourself

```bash
# from a tree that can already link against libdekaf2 (adjust the -isystem/-L to taste):
clang++ -std=gnu++2b -O2 \
  -isystem /usr/local/include/dekaf2 \
  pending-patches/kparsetimestamp_hour12_repro.cpp \
  /usr/local/lib/dekaf2/Release/libdekaf2.a $(pkg-config --libs fmt 2>/dev/null) \
  -o /tmp/repro && /tmp/repro
```

(The version I ran links the full paxapis lib set; the single-line above is the minimal
idea. Output is committed as `kparsetimestamp_hour12.evidence.txt` so you can diff against
your own run.)

## Why it matters (real-world impact)

FreeTDS / CT-Lib renders a SQL Server **`date`** column in Sybase 12-hour spelling, and a
date column has no time, so it **always** renders midnight as `12:00:00:AM` — precisely the
value `kParseTimestamp` rejects. Any dekaf2 caller normalizing SQL Server date columns
through `kParseTimestamp` will fail on effectively *every* date-typed value.

paxapis' warehouse sync hit this on `ProjectA_ClientCompany.INFLATIONVERIFICATIONEXPIRATIONDATE`
and now hand-rolls a Sybase→ISO converter in `paxapis/paxsync.cpp`
(`ConvertSybaseTimestamp`) as a workaround, explicitly *because* `kParseTimestamp` can't
take the 12-o'clock hour. Once this lands, that workaround can lean on dekaf2 again.

## Merge checklist

- [ ] `git apply pending-patches/kparsetimestamp_hour12.patch`
- [ ] Build the repro, confirm all 24 rows now parse (12 AM → 00:00:00, 12 PM → 12:00:00)
- [ ] Add a unit test to the ktime test suite covering 12 AM / 12 PM in both `aa` and
      `h/hh` variants (the format table has several 12-hour entries; 12-o'clock was simply
      never exercised)
