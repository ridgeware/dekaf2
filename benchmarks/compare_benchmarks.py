#!/usr/bin/env python3
#
# compare_benchmarks.py - compare two benchmark JSON result files
#
# usage:
#   compare_benchmarks.py <baseline.json> <current.json> [--ranges <ranges.json>] [--threshold <percent>]
#
# exit codes:
#   0 = all ok (or only improvements)
#   1 = regressions detected
#   2 = usage error
#

import json
import sys
import os
import argparse

# ANSI color codes
RED    = "\033[91m"
GREEN  = "\033[92m"
YELLOW = "\033[93m"
BOLD   = "\033[1m"
RESET  = "\033[0m"

def load_json(pathname):
    with open(pathname, "r") as f:
        return json.load(f)

def format_nsec(nsec):
    if nsec >= 1_000_000:
        return f"{nsec / 1_000_000:.2f} ms"
    elif nsec >= 1_000:
        return f"{nsec / 1_000:.1f} µs"
    else:
        return f"{nsec} ns"

def compare(baseline, current, ranges, threshold_pct):
    """Compare two benchmark result sets. Returns number of regressions."""

    base_map = {}
    for r in baseline.get("results", []):
        if not r.get("is_group", False) and "nsec_per_call" in r:
            base_map[r["label"]] = r

    curr_map = {}
    for r in current.get("results", []):
        if not r.get("is_group", False) and "nsec_per_call" in r:
            curr_map[r["label"]] = r

    all_labels = []
    seen = set()
    for r in baseline.get("results", []) + current.get("results", []):
        label = r.get("label", "")
        if label not in seen and not r.get("is_group", False) and "nsec_per_call" in r:
            all_labels.append(label)
            seen.add(label)

    regressions  = 0
    improvements = 0
    unchanged    = 0
    range_fails  = 0

    # header
    print()
    print(f"{BOLD}{'Benchmark':<50} {'Baseline':>12} {'Current':>12} {'Change':>10} {'Status':>10}{RESET}")
    print("-" * 100)

    for label in all_labels:
        base = base_map.get(label)
        curr = curr_map.get(label)

        if base and curr:
            b_nsec = base["nsec_per_call"]
            c_nsec = curr["nsec_per_call"]

            if b_nsec > 0:
                change_pct = ((c_nsec - b_nsec) / b_nsec) * 100.0
            else:
                change_pct = 0.0

            b_str = format_nsec(b_nsec)
            c_str = format_nsec(c_nsec)

            if change_pct > threshold_pct:
                status = f"{RED}REGRESSION{RESET}"
                regressions += 1
            elif change_pct < -threshold_pct:
                status = f"{GREEN}IMPROVED{RESET}"
                improvements += 1
            else:
                status = "ok"
                unchanged += 1

            sign = "+" if change_pct >= 0 else ""
            print(f"  {label:<48} {b_str:>12} {c_str:>12} {sign}{change_pct:>8.1f}% {status:>10}")

        elif base and not curr:
            b_str = format_nsec(base["nsec_per_call"])
            print(f"  {label:<48} {b_str:>12} {'(missing)':>12} {'':>10} {YELLOW}REMOVED{RESET}")

        elif curr and not base:
            c_str = format_nsec(curr["nsec_per_call"])
            print(f"  {label:<48} {'(new)':>12} {c_str:>12} {'':>10} {YELLOW}NEW{RESET}")

    # check expected ranges
    if ranges:
        print()
        print(f"{BOLD}Expected Range Checks:{RESET}")
        print("-" * 100)

        for label, spec in ranges.items():
            curr = curr_map.get(label)
            if not curr:
                continue

            c_nsec  = curr["nsec_per_call"]
            min_ns  = spec.get("min_nsec", 0)
            max_ns  = spec.get("max_nsec", float("inf"))

            c_str   = format_nsec(c_nsec)
            min_str = format_nsec(min_ns)
            max_str = format_nsec(max_ns) if max_ns != float("inf") else "inf"

            if c_nsec < min_ns:
                status = f"{YELLOW}BELOW MIN{RESET}"
                range_fails += 1
            elif c_nsec > max_ns:
                status = f"{RED}ABOVE MAX{RESET}"
                range_fails += 1
            else:
                status = f"{GREEN}in range{RESET}"

            print(f"  {label:<48} {c_str:>12}   [{min_str} .. {max_str}]  {status}")

    # summary
    print()
    print(f"{BOLD}Summary:{RESET}")
    print(f"  Baseline:  {baseline.get('timestamp', '?')}")
    print(f"  Current:   {current.get('timestamp', '?')}")
    print(f"  Threshold: {threshold_pct:.1f}%")
    print(f"  Results:   {improvements} improved, {unchanged} unchanged, {regressions} regressed")

    if ranges:
        print(f"  Ranges:    {range_fails} out of range")

    print()

    return regressions + range_fails

def main():
    parser = argparse.ArgumentParser(
        description="Compare two dekaf2 benchmark JSON result files"
    )
    parser.add_argument("baseline", help="baseline JSON file")
    parser.add_argument("current",  help="current JSON file")
    parser.add_argument("--ranges", "-r",
        help="expected target ranges JSON file",
        default=None
    )
    parser.add_argument("--threshold", "-t",
        help="regression threshold in percent (default: 10.0)",
        type=float,
        default=10.0
    )

    args = parser.parse_args()

    if not os.path.isfile(args.baseline):
        print(f"error: baseline file not found: {args.baseline}", file=sys.stderr)
        return 2

    if not os.path.isfile(args.current):
        print(f"error: current file not found: {args.current}", file=sys.stderr)
        return 2

    baseline = load_json(args.baseline)
    current  = load_json(args.current)

    ranges = None
    if args.ranges:
        if not os.path.isfile(args.ranges):
            print(f"error: ranges file not found: {args.ranges}", file=sys.stderr)
            return 2
        ranges = load_json(args.ranges)

    failures = compare(baseline, current, ranges, args.threshold)

    return 1 if failures > 0 else 0

if __name__ == "__main__":
    sys.exit(main())
