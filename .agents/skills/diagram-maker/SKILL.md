---
name: diagram-maker
description: Create bespoke standalone SVG/HTML visuals or editable Excalidraw whiteboards when the user explicitly needs custom positioning, trust zones, swimlanes, hub-spoke layouts, or Excalidraw output. Do not use for routine flowcharts, sequence/state/class diagrams, ERDs, XY charts, or other diagrams that Pretty Mermaid can render from concise Mermaid source.
---

# Diagram Maker

Create diagrams as artifacts, not prose. This skill complements the repository's
`pretty-mermaid` skill; it does not replace it.

## Routing boundary

Use Pretty Mermaid by default for routine source-controlled flowcharts,
sequence/state/class diagrams, ERDs, XY charts, and architecture or workflow
diagrams that fit concise Mermaid source.

Use this skill only when at least one of these is true:

- The user explicitly requests an editable `.excalidraw` file.
- The visual requires precise manual placement, trust zones, swimlanes,
  hub-spoke composition, or another layout Mermaid cannot express clearly.
- A Mermaid draft has been inspected and its automatic layout is materially
  unclear, and the user wants a bespoke standalone artifact.

Excalidraw is file-based editability, not a shared FigJam board. For an
interactive or animated visual, use the appropriate visualization workflow
instead of embedding JavaScript here.

## Output modes

Choose one output mode:

- `clean-svg`: bespoke educational concepts, physical systems, or processes.
- `architecture-svg`: software/cloud/infra topology, services, databases, queues, trust zones.
- `excalidraw`: editable hand-drawn whiteboard or architecture sketch.

Workflow

1. Extract nodes, groups, labels, and directed relationships.
2. Pick layout first: left-to-right, top-down, hub-spoke, swimlanes, layered stack, sequence.
3. Keep labels short. Prefer 5-9 main elements over dense diagrams.
4. Generate the file at the requested path. In this repository, prefer
   `docs/visualizations/<name>.html` for standalone SVG/HTML and
   `docs/diagrams/<name>.excalidraw` for Excalidraw sources.
5. Keep observations visually distinct from explanations or hypotheses.
6. Verify syntax and inspect the result before handing it off.

SVG/HTML rules

- Single standalone `.html` file with inline CSS and inline SVG.
- No external fonts, JS, images, gradients, glows, decorative blobs, or remote assets.
- Use semantic colors, not rainbow sequences: neutral, input, process, storage, external, risk.
- Draw connectors before nodes so arrows sit behind boxes.
- Every connector path has `fill="none"` and a marker arrow when directed.
- Leave 24px text padding inside boxes; do not let text touch borders.
- Legend only when symbols/colors are not obvious.
- Parse the completed HTML and inspect the rendered SVG for clipping,
  collisions, and ambiguous connectors.

SVG template

Use `references/svg-template.md` as the wrapper and replace `<!-- SVG -->`.

Excalidraw rules

- Save `.excalidraw` JSON with `type`, `version`, `source`, `elements`, and `appState`.
- Use bound text for shape labels. Do not use a nonstandard `label` property.
- Keep bound text immediately after its container in the elements array.
- Minimum labeled shape: 120x60. Minimum body text: 16px.
- Use roughness `1`, `fontFamily: 1`, and simple fills.
- Parse the completed JSON. Check that element IDs are unique and that bound
  element and container references resolve.

For exact Excalidraw element snippets, read `references/excalidraw-patterns.md`.
