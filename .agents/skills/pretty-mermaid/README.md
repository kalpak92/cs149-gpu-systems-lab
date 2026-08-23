# Pretty Mermaid — project-local skill

This directory contains the functional subset of
[Pretty Mermaid](https://github.com/imxv/Pretty-mermaid-skills) used by this
repository. It renders editable Mermaid source as themed SVG, PNG, or terminal
text without a browser.

Read [`SKILL.md`](SKILL.md) for the authoritative workflow and project-specific
safety rules. Upstream provenance and local adaptations are recorded in
[`UPSTREAM.md`](UPSTREAM.md).

## Recreate the renderer environment

After explaining the dependency boundary and receiving approval:

```bash
npm ci
```

Dependencies are installed only inside this directory and `node_modules/`
remains ignored.

## Validate the vendored subset

```bash
npm test
npm run validate
```

The first command exercises every supported diagram type and output format.
The second validates the skill instructions, local documentation links, and
the complete theme gallery.
