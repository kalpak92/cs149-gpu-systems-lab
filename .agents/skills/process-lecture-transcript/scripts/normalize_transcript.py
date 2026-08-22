#!/usr/bin/env python3
"""Normalize timestamped bracket, VTT, or SRT lecture transcripts."""

from __future__ import annotations

import argparse
import html
import re
import sys
from dataclasses import dataclass
from pathlib import Path


BRACKETED = re.compile(r"^\[(\d{1,2}:\d{2}(?::\d{2})?)\]\s*(.*)$")
TIMECODE = re.compile(
    r"^(\d{1,2}:\d{2}:\d{2}[.,]\d{3}|\d{1,2}:\d{2}[.,]\d{3})\s+-->"
)
TAG = re.compile(r"<[^>]+>")


@dataclass
class Segment:
    seconds: float
    text: str


def parse_clock(value: str) -> float:
    normalized = value.replace(",", ".")
    parts = normalized.split(":")
    if len(parts) == 2:
        minutes, seconds = parts
        return (int(minutes) * 60) + float(seconds)
    if len(parts) == 3:
        hours, minutes, seconds = parts
        return (int(hours) * 3600) + (int(minutes) * 60) + float(seconds)
    raise ValueError(f"unsupported timestamp: {value}")


def format_clock(seconds: float) -> str:
    total = max(0, int(seconds))
    hours, remainder = divmod(total, 3600)
    minutes, secs = divmod(remainder, 60)
    return f"{hours:02d}:{minutes:02d}:{secs:02d}"


def clean_caption_text(value: str) -> str:
    without_tags = TAG.sub("", html.unescape(value))
    return " ".join(without_tags.split())


def parse_bracketed(lines: list[str]) -> list[Segment]:
    segments = []
    for line in lines:
        match = BRACKETED.match(line.strip())
        if not match:
            continue
        text = clean_caption_text(match.group(2))
        if text:
            segments.append(Segment(parse_clock(match.group(1)), text))
    return segments


def parse_vtt_or_srt(lines: list[str]) -> list[Segment]:
    segments = []
    index = 0
    while index < len(lines):
        match = TIMECODE.match(lines[index].strip())
        if not match:
            index += 1
            continue
        start = parse_clock(match.group(1))
        index += 1
        caption_lines = []
        while index < len(lines):
            candidate = lines[index].strip()
            if TIMECODE.match(candidate):
                break
            if not candidate:
                index += 1
                break
            if not candidate.isdigit() and not candidate.startswith(("NOTE", "STYLE")):
                caption_lines.append(candidate)
            index += 1
        text = clean_caption_text(" ".join(caption_lines))
        if text:
            segments.append(Segment(start, text))
    return segments


def parse_segments(path: Path) -> list[Segment]:
    lines = path.read_text(encoding="utf-8").splitlines()
    segments = parse_bracketed(lines)
    if segments:
        return segments
    segments = parse_vtt_or_srt(lines)
    if segments:
        return segments
    raise ValueError(
        "no timestamped segments found; supply bracketed timestamps, VTT, or SRT"
    )


def collapse_tandem_repeats(
    words: list[str], min_width: int, max_width: int
) -> tuple[list[str], int]:
    removed = 0
    changed = True
    while changed:
        changed = False
        upper = min(max_width, len(words) // 2)
        for width in range(upper, min_width - 1, -1):
            index = 0
            while index + (2 * width) <= len(words):
                first = words[index : index + width]
                second = words[index + width : index + (2 * width)]
                if first == second:
                    del words[index + width : index + (2 * width)]
                    removed += width
                    changed = True
                else:
                    index += 1
            if changed:
                break
    return words, removed


def remove_cross_segment_overlap(
    segments: list[Segment], min_width: int, max_width: int
) -> tuple[list[Segment], int]:
    result = []
    removed = 0
    previous_words: list[str] = []
    for segment in segments:
        words = segment.text.split()
        upper = min(max_width, len(previous_words), len(words))
        overlap = 0
        for width in range(upper, min_width - 1, -1):
            if previous_words[-width:] == words[:width]:
                overlap = width
                break
        if overlap:
            words = words[overlap:]
            removed += overlap
        if words:
            result.append(Segment(segment.seconds, " ".join(words)))
            previous_words = words
    return result, removed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--output", "-o", required=True, type=Path)
    parser.add_argument("--collapse-tandem-repeats", action="store_true")
    parser.add_argument("--remove-cross-segment-overlap", action="store_true")
    parser.add_argument("--min-repeat-words", type=int, default=3)
    parser.add_argument("--max-repeat-words", type=int, default=60)
    parser.add_argument("--max-removal-ratio", type=float, default=0.80)
    args = parser.parse_args()

    if args.input.resolve() == args.output.resolve():
        parser.error("output must differ from input; never overwrite raw evidence")
    if args.min_repeat_words < 2:
        parser.error("--min-repeat-words must be at least 2")
    if args.max_repeat_words < args.min_repeat_words:
        parser.error("--max-repeat-words must be >= --min-repeat-words")

    try:
        segments = parse_segments(args.input)
    except (OSError, UnicodeError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    input_segments = len(segments)
    input_words = sum(len(segment.text.split()) for segment in segments)
    tandem_removed = 0
    overlap_removed = 0

    if args.collapse_tandem_repeats:
        normalized = []
        for segment in segments:
            words, removed = collapse_tandem_repeats(
                segment.text.split(), args.min_repeat_words, args.max_repeat_words
            )
            normalized.append(Segment(segment.seconds, " ".join(words)))
            tandem_removed += removed
        segments = normalized

    if args.remove_cross_segment_overlap:
        segments, overlap_removed = remove_cross_segment_overlap(
            segments, args.min_repeat_words, args.max_repeat_words
        )

    output_words = sum(len(segment.text.split()) for segment in segments)
    removal_ratio = 0.0 if input_words == 0 else 1 - (output_words / input_words)
    if removal_ratio > args.max_removal_ratio:
        print(
            f"error: normalization would remove {removal_ratio:.1%} of words, "
            f"exceeding --max-removal-ratio {args.max_removal_ratio:.1%}",
            file=sys.stderr,
        )
        return 2

    args.output.write_text(
        "".join(
            f"[{format_clock(segment.seconds)}] {segment.text}\n"
            for segment in segments
        ),
        encoding="utf-8",
    )

    print(f"input segments: {input_segments}")
    print(f"output segments: {len(segments)}")
    print(f"input words: {input_words}")
    print(f"output words: {output_words}")
    print(f"tandem-repeat words removed: {tandem_removed}")
    print(f"cross-segment overlap words removed: {overlap_removed}")
    print(f"total removal ratio: {removal_ratio:.1%}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
