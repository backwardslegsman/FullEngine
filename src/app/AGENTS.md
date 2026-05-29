# src/app/AGENTS.md

## SDL3 app shell rules

SDL3 belongs in the platform/app layer unless a renderer function explicitly requires a native window handle.

A sample app may use SDL3 directly for:

- window creation
- event polling
- keyboard/mouse input
- resize events
- relative mouse mode
- high-DPI handling

Do not let SDL input types leak into renderer scene APIs.

## Sample app role

The sample app demonstrates integration with the renderer. It must not become the engine.

Sample app code may own demo camera controls, demo scene setup, command-line flags, temporary sample assets, and simple smoke-test scenes.

Do not move reusable renderer logic into `src/app/`. Move reusable logic into the appropriate renderer subtree.

For Phase 3 hardening, the sample should become a repeatable validation harness:
feature presets, camera bookmarks, independent toggles for optional passes,
stress scenes for terrain/instances/decals/particles/skinned actors, and smoke
launch coverage. Sample-owned systems such as selection, weather, particles,
and structure fade must remain clearly outside renderer core.

## Window/platform data

The renderer init descriptor may accept platform-native window data where practical. Prefer neutral platform descriptors over SDL-specific public API types.

Window lifetime must exceed renderer backend lifetime.
