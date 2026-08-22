#!/usr/bin/env python3
"""Check transcript timestamp coverage and manage a chronological review ledger."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


TIMESTAMP = re.compile(r"^\[(\d{1,2}:\d{2}(?::\d{2})?)\]\s+\S")
LEDGER_ITEM = re.compile(r"^- \[([ xX])\] (\d{2}:\d{2}:\d{2})-(\d{2}:\d{2}:\d{2})")


def parse_clock(value: str) -> int:
    parts = [int(part) for part in value.split(":")]
    if len(parts) == 2:
        return (parts[0] * 60) + parts[1]
    if len(parts) == 3:
        return (parts[0] * 3600) + (parts[1] * 60) + parts[2]
    raise argparse.ArgumentTypeError(f"invalid duration or timestamp: {value}")


def format_clock(seconds: int) -> str:
    hours, remainder = divmod(max(0, seconds), 3600)
    minutes, secs = divmod(remainder, 60)
    return f"{hours:02d}:{minutes:02d}:{secs:02d}"


def read_timestamps(path: Path) -> list[int]:
    timestamps = []
    for line in path.read_text(encoding="utf-8").splitlines():
        match = TIMESTAMP.match(line.strip())
        if match:
            timestamps.append(parse_clock(match.group(1)))
    if not timestamps:
        raise ValueError("no non-empty bracketed timestamp segments found")
    return timestamps


def coverage_issues(
    timestamps: list[int], expected_duration: int | None, max_gap: int
) -> list[str]:
    issues = []
    regressions = [
        (left, right)
        for left, right in zip(timestamps, timestamps[1:])
        if right < left
    ]
    if regressions:
        issues.append(f"timestamp order regresses {len(regressions)} time(s)")

    if timestamps[0] > max_gap:
        issues.append(
            f"first segment begins at {format_clock(timestamps[0])}, "
            f"more than {max_gap}s after the start"
        )

    internal_gaps = [
        (left, right)
        for left, right in zip(timestamps, timestamps[1:])
        if right - left > max_gap
    ]
    for left, right in internal_gaps:
        issues.append(
            f"internal gap {format_clock(left)}-{format_clock(right)} "
            f"is {right - left}s"
        )

    if expected_duration is not None:
        if timestamps[-1] > expected_duration + max_gap:
            issues.append(
                f"last timestamp {format_clock(timestamps[-1])} exceeds expected "
                f"duration {format_clock(expected_duration)} by more than {max_gap}s"
            )
        elif expected_duration - timestamps[-1] > max_gap:
            issues.append(
                f"last segment at {format_clock(timestamps[-1])} ends more than "
                f"{max_gap}s before expected duration {format_clock(expected_duration)}"
            )
    return issues


def ledger_rows(
    timestamps: list[int], duration: int, chunk_seconds: int
) -> list[tuple[int, int, int]]:
    rows = []
    for start in range(0, duration, chunk_seconds):
        end = min(duration, start + chunk_seconds)
        count = sum(start <= timestamp < end for timestamp in timestamps)
        rows.append((start, end, count))
    return rows


def write_ledger(
    path: Path, transcript: Path, duration: int, rows: list[tuple[int, int, int]]
) -> None:
    lines = [
        "# Transcript review ledger",
        "",
        f"Transcript: `{transcript}`",
        f"Expected duration: `{format_clock(duration)}`",
        "",
        "Mark an interval complete only after reading every segment in it.",
        "",
    ]
    for start, end, count in rows:
        lines.append(
            f"- [ ] {format_clock(start)}-{format_clock(end)} "
            f"({count} timestamped segments)"
        )
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def verify_ledger(
    path: Path, expected_rows: list[tuple[int, int, int]]
) -> list[str]:
    items = []
    for line in path.read_text(encoding="utf-8").splitlines():
        match = LEDGER_ITEM.match(line)
        if match:
            items.append(
                (
                    match.group(1).lower() == "x",
                    parse_clock(match.group(2)),
                    parse_clock(match.group(3)),
                )
            )
    issues = []
    if len(items) != len(expected_rows):
        issues.append(
            f"ledger has {len(items)} interval row(s); expected {len(expected_rows)}"
        )
    actual_ranges = [(start, end) for _, start, end in items]
    wanted_ranges = [(start, end) for start, end, _ in expected_rows]
    if actual_ranges != wanted_ranges:
        issues.append("ledger interval ranges do not match the expected review chunks")
    incomplete = sum(not done for done, _, _ in items)
    if incomplete:
        issues.append(f"ledger has {incomplete} incomplete interval(s)")
    return issues


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("transcript", type=Path)
    parser.add_argument("--expected-duration", type=parse_clock)
    parser.add_argument("--max-gap-seconds", type=int, default=90)
    parser.add_argument("--chunk-minutes", type=int, default=10)
    parser.add_argument("--ledger", type=Path)
    parser.add_argument("--verify-ledger", type=Path)
    args = parser.parse_args()

    if args.max_gap_seconds <= 0:
        parser.error("--max-gap-seconds must be positive")
    if args.chunk_minutes <= 0:
        parser.error("--chunk-minutes must be positive")
    if (args.ledger or args.verify_ledger) and args.expected_duration is None:
        parser.error("--expected-duration is required for ledger operations")
    if args.ledger and args.ledger.resolve() == args.transcript.resolve():
        parser.error("--ledger must not overwrite the transcript")

    try:
        timestamps = read_timestamps(args.transcript)
    except (OSError, UnicodeError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    gaps = [right - left for left, right in zip(timestamps, timestamps[1:])]
    maximum_gap = max(gaps, default=0)
    issues = coverage_issues(
        timestamps, args.expected_duration, args.max_gap_seconds
    )

    print(f"segments: {len(timestamps)}")
    print(f"first timestamp: {format_clock(timestamps[0])}")
    print(f"last timestamp: {format_clock(timestamps[-1])}")
    print(f"maximum internal gap: {maximum_gap}s")

    duration = args.expected_duration
    rows = []
    if duration is not None:
        rows = ledger_rows(timestamps, duration, args.chunk_minutes * 60)
        empty_rows = [row for row in rows if row[2] == 0]
        if empty_rows:
            issues.append(f"{len(empty_rows)} ledger interval(s) contain no segments")

    if args.ledger:
        write_ledger(args.ledger, args.transcript, duration, rows)
        print(f"ledger written: {args.ledger}")

    if args.verify_ledger:
        try:
            issues.extend(verify_ledger(args.verify_ledger, rows))
        except (OSError, UnicodeError) as error:
            print(f"error: {error}", file=sys.stderr)
            return 2

    if issues:
        print("coverage: FAIL")
        for issue in issues:
            print(f"- {issue}")
        return 1

    print("coverage: PASS")
    if args.verify_ledger:
        print("ledger: COMPLETE")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
