# Note Chain Assist Modularization Prep

Status: prep only, no module code extraction started yet.

## Goal

Split `plugins/builtin/note_chain_assist/note_chain_assist.py` into smaller modules without behavior changes, with sidecar V3 behavior (`format_version=3`, CAS conflict check, stable `curve_id`, unique `curve_no`) kept intact.

## Proposed Module Boundaries

1. `core/state.py`
- `STATE` schema, defaults, snapshot capture/restore.
- Common state helpers (`_clone`, id counters, cache invalidation).

2. `core/time_math.py`
- triplet/beat conversion, clamping, geometry and interpolation helpers.
- lane/time snapping and beat-window helpers.

3. `core/curve_model.py`
- anchor/link normalization and cleanup.
- segment density/style getters/setters.
- selection/link operations and connected-segment traversal.

4. `core/sidecar_v3.py`
- v2/v3 load + migration.
- v3 payload build and metadata cleanup.
- CAS save flow and atomic replace.
- curve numbering self-heal and group name uniqueness repair.

5. `ui/overlay.py`
- overlay item composition for preview/handles/samples/labels.

6. `ui/input_handler.py`
- mouse/wheel/key handling.
- drag state transitions, selection box, handle edit, context-hit behavior.

7. `actions/tool_actions.py`
- tool action list construction and dispatch.
- density/shape/group related action endpoints.

8. `actions/batch_commit.py`
- `Commit Curve -> Notes` generation and duplicate note-position filtering.

9. `runtime/plugin_loop.py`
- protocol dispatch (`listToolActions`, `runToolAction`, `listCanvasOverlays`, etc.).
- notify event handling (`onChartSaved`, `shutdown`, undo/redo hooks).

## Dependency Direction (target)

`runtime` -> `actions` -> (`ui`, `core`) -> `core/state`

Rules:
- `core/*` must not import from `ui/*` or `runtime/*`.
- `sidecar_v3.py` can depend on `core/state.py` and `core/curve_model.py`, but not input/render layers.
- `plugin_loop.py` should be the only place aware of transport protocol details.

## Preparation Checklist (before extraction)

1. Freeze behavior contracts:
- Sidecar V3 field names and migration behavior.
- CAS conflict behavior (`revision_conflict`).
- Group name uniqueness (case-insensitive) auto-fix behavior.

2. Add seam map comments in current monolith:
- Mark start/end blocks for sidecar, overlay, input, actions, runtime.

3. Define stable function ownership table:
- each existing function mapped to one target module.
- identify 10-15 cross-cutting helpers that must move first (`_parse_int`, `_normalize_link`, `_float_to_triplet`, etc.).

4. Define import-safe state access pattern:
- either explicit state object pass-through or one centralized state accessor.
- avoid circular imports during extraction.

5. Define extraction order:
- order must keep plugin runnable after each small step.

## Extraction Order (implementation phase, later)

1. Extract pure math/time helpers (`core/time_math.py`).
2. Extract sidecar codec and CAS save (`core/sidecar_v3.py`).
3. Extract curve/link model operations (`core/curve_model.py`).
4. Extract action dispatch and batch commit logic.
5. Extract overlay builder and input handler.
6. Finalize protocol loop wrapper and shrink monolith entry file.

## Acceptance Criteria For Modularization (later)

1. Functional parity:
- same editing behavior for anchor/handle drag and selection.
- same `Commit Curve -> Notes` result given same state.

2. Persistence parity:
- v2 loads correctly, saves as v3.
- `curve_no` uniqueness and self-heal still pass.
- CAS conflict still rejects stale save.

3. Code structure:
- monolith entry reduced to orchestration only.
- no circular imports.
- module-level responsibilities align with boundary list above.

4. Regression sanity:
- run minimal plugin smoke checks and existing tests touching plugin bridge.

