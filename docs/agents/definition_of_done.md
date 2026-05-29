# Definition of Done

A renderer change is complete when the relevant items below are satisfied. Apply judgment for documentation-only or exploratory changes, but do not skip build/test validation for code changes without saying why.

## General code changes

- The project builds.
- The change is scoped to the requested behavior.
- Existing sample scenes or smoke tests are not broken.
- Public APIs have Doxygen-compatible comments.
- Resource lifetimes are explicit.
- Errors are handled or asserted intentionally.
- CPU-side tests are added for algorithmic logic where practical.
- Docs are updated for engine-facing behavior.

## Public API changes

Public APIs must use Doxygen-compatible comments and document:

- ownership
- lifetime
- thread expectations
- units and coordinate conventions
- whether data is copied, referenced, or consumed
- whether calls are valid before, during, or after a frame
- parameter semantics, return values, and error behavior

Public APIs should prefer opaque handles over raw pointers for renderer-owned resources. Public header comments should be sufficient for generated Doxygen API reference output.

## Backend changes

Backend changes are complete when:

- backend-specific code stays behind the backend layer
- initialization and shutdown paths are covered
- resize behavior is considered
- resource destruction is safe and ordered
- failure modes produce structured errors or clear assertions

## Rendering feature changes

Rendering feature changes are complete when:

- the feature can be enabled or configured through documented renderer data
- shader inputs and CPU-side descriptors agree
- color space expectations are explicit
- debug visibility exists where appropriate
- the feature does not force unrelated passes to exist

## Phase 3 hardening changes

Phase 3 hardening changes are complete when:

- prior interaction bugs have regression coverage where practical
- optional passes can be independently disabled without breaking presentation
- scene color/depth ownership and resize behavior are documented
- invalid/stale handles and missing resources fail safely
- debug stats explain submitted, skipped, culled, rejected, and invalid work
- sample validation scenes or smoke checks cover affected feature combinations
- no gameplay ownership, editor state, or backend handles leak into public APIs

## Open-world feature changes

Terrain, culling, LOD, instancing, and streaming changes are complete when:

- ownership and lifetime are stable under create/destroy cycles
- non-resident data is handled intentionally
- chunk/object bounds are inspectable
- debug views can explain culling or LOD decisions where practical
- CPU-side algorithms have tests when practical
- the change states whether it improves prototype integration only or closes a
  production open-world readiness gate such as streaming/residency,
  large-world precision, asset validation, shader packaging, performance proof,
  platform coverage, or long-session stability

## Documentation-only changes

Documentation-only changes are complete when:

- docs do not claim future work is already implemented
- examples match the public API or are clearly marked as pseudocode
- cross-links to relevant docs are updated
- Doxygen guidance in `docs/agents/doxygen_style.md` remains synchronized with public API documentation expectations
