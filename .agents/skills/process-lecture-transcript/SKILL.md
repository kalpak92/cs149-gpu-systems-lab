---
name: process-lecture-transcript
description: Acquire, normalize, review completely, and reconcile lecture or technical-video transcripts into evidence-backed notes. Use when Codex needs to acquire or clean a transcript, review a complete video chronologically, verify transcript coverage, reconcile notes with spoken content, slides, or primary sources, remove rolling auto-caption duplication, or make a source-sensitive technical correction. Do not use for routine wording, formatting, organization, or diagram edits to existing notes when reopening transcript or source evidence is unnecessary.
---

# Process Lecture Transcript

Turn lecture transcripts into technically accurate notes without confusing
caption text, source material, interpretation, or the learner's reflection.

## Evidence contract

- Preserve the raw transcript. Never normalize in place.
- Record the video URL, duration, language, and whether captions are automatic
  or author-provided when those facts are available.
- Treat automatic captions as navigation evidence, not exact quotations.
- Do not infer missing speech when captions are unavailable or incomplete.
- Do not claim a complete review after keyword searches or selected excerpts.
- Reserve personal takeaways and reflections for the learner unless the user
  explicitly supplies them.
- Prefer authoritative committed project files over older chat context.

## Workflow

### 1. Establish the requested output

Read applicable `AGENTS.md` files and the existing note before processing
sources. Determine whether the user wants a review, a draft, an edit, a topic
map, or a source reconciliation. Do not modify files when the request is only
to assess or report.

### 2. Acquire and preserve sources

Collect, in priority order:

1. The referenced video or lecture page.
2. Author-provided slides, notes, or course page.
3. Native captions or a user-supplied timestamped transcript.
4. A public transcript fallback only when native captions are unavailable.

Keep acquisition flexible because video providers and caption endpoints vary.
If a network or authentication boundary blocks acquisition, report it and ask
for the transcript rather than fabricating content. Store temporary raw data
outside the repository unless the user asks to preserve it as an artifact.

### 3. Inspect before normalizing

Check the raw file's size, line count, first timestamp, last timestamp, and a
sample from the beginning, middle, and end. Identify whether it is bracketed
text, VTT, SRT, or untimestamped prose.

When rolling captions repeat exact phrases, run:

```bash
python3 scripts/normalize_transcript.py RAW \
  --output CLEAN \
  --collapse-tandem-repeats \
  --remove-cross-segment-overlap
```

Omit deduplication flags when the raw transcript does not demonstrate those
artifacts. Compare reported word counts and samples with the raw source. Treat
large or surprising reductions as a reason to inspect, not as success.

### 4. Generate the coverage ledger

Run:

```bash
python3 scripts/transcript_coverage.py CLEAN \
  --expected-duration H:MM:SS \
  --chunk-minutes 10 \
  --ledger /tmp/lecture-review-ledger.md
```

Resolve reported start, end, or internal-gap failures before continuing. A
coverage check shows that timestamped text spans the video; it does not prove
that the transcript was read.

### 5. Review every interval

Read the normalized transcript chronologically. Use chunks small enough to fit
comfortably in context. For each ledger interval:

1. Read every transcript segment in the interval.
2. Record the main claim, example, transition, and any uncertainty.
3. Mark that interval `[x]` only after reading it.

Do not replace this pass with `grep`, topic searches, a generated summary, or
slides-only review. Searches may follow the complete pass to locate details.

Verify the finished ledger:

```bash
python3 scripts/transcript_coverage.py CLEAN \
  --expected-duration H:MM:SS \
  --verify-ledger /tmp/lecture-review-ledger.md
```

Only state that the complete transcript was reviewed when transcript coverage
passes and ledger verification reports every interval complete.

### 6. Build a source map

Create a timestamped topic map before rewriting prose. For each major section,
track:

- timestamp range;
- spoken claim or example;
- supporting slide/page when available;
- conflicts or caption uncertainty;
- interpretation or infrastructure connection.

When a supplied outline disagrees with the transcript or video, preserve and
report the discrepancy. Do not silently pick the convenient timestamp.

### 7. Reconcile sources and notes

Read [references/source-reconciliation.md](references/source-reconciliation.md)
before editing notes or making technical claims. Organize the final note for
learning, but retain a compact chronological map so source coverage remains
auditable.

Use explicit labels or wording to separate:

- observed video/transcript content;
- slide or course-page content;
- technical interpretation;
- later-course preview;
- learner reflection.

If slides continue beyond the linked video's stopping point, label that
boundary rather than attributing slide-only content to the video.

### 8. Validate and report

Run repository-appropriate formatting checks. Report:

- transcript range reviewed;
- whether captions were automatic;
- primary sources used;
- substantive note changes;
- unresolved uncertainties;
- files changed and whether anything was committed.

Do not report transcript acquisition or normalization as equivalent to review.

## Resource routing

- Use `scripts/normalize_transcript.py` for bracketed timestamps, VTT, or SRT
  and for measured removal of demonstrated rolling-caption duplication.
- Use `scripts/transcript_coverage.py` to inspect timestamp coverage, generate
  review ledgers, and verify completed ledgers.
- Read `references/source-reconciliation.md` before comparing transcript,
  slides, existing notes, and interpretation.
