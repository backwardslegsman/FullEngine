# .vscode/AGENTS.md

## VSCode guidance

Prefer repository-local configuration only when useful.

Useful files:

```text
.vscode/
  settings.json
  tasks.json
  launch.json
  extensions.json
```

Recommended VSCode tasks:

- configure CMake
- build
- run sample app
- compile shaders
- run tests
- format changed files

Do not hard-code user-specific absolute paths in VSCode settings.

Use workspace-relative paths and existing build presets where possible.
