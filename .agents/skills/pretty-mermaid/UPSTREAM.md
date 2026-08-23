# Upstream provenance

- Repository: <https://github.com/imxv/Pretty-mermaid-skills>
- Vendored commit: `e234ab73c2992f03c36db04fd55ebb90abca9c45`
- Vendored on: 2026-08-23
- License: MIT; see `LICENSE`.

This is a functional subset containing the skill instructions, renderer,
tests, references, examples, and theme gallery. Upstream repository-maintenance
files and promotional assets are intentionally omitted.

## Project-local adaptations

- Removed automatic `npm install` calls from the render, batch, and theme
  scripts.
- Pinned direct dependency versions exactly and retained the upstream lockfile.
- Removed external font imports from generated SVGs so documentation assets are
  self-contained.
- Retained the upstream theme preview SVGs when vendored into this parent
  repository.
- Replaced the upstream promotional and translated READMEs with one concise
  project-local README, and updated the documentation validator accordingly.
- Added repository-specific source, output, theme, and approval conventions to
  `SKILL.md`.

Review upstream changes and reapply these adaptations deliberately when
updating the vendored snapshot.
