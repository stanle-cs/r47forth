# G3/G4 fix-design review — continuation of Claude's interrupted review

Date: 2026-09-05. Status: design review complete; implementation and runtime acceptance are separate work.

## Verdict

Do not apply the saved proposals verbatim. The repair directions are mostly sound, but the proposals omit failure paths, contain fixtures that cannot reach their claimed state, and disagree on test names and expected behavior. This document supplies the missing engine design and consolidates the required amendments.

Recommended behavior: refuse a 3D run when its undo image cannot be saved; protect every retained-grid write; validate the floating-point window only when 3D needs it; preserve mirrored windows; reset the transform when content is explicitly cleared; keep navigation a no-op when nothing is retained. Before serialization, release canvas resources even when a plot has already changed the calculator mode. These are recommendations for the implementation packet, not claims that the changes have been applied.

## Evidence and scope

- Recovered seven successful design results from Claude workflow `wf_985cb046-556`; its engine designer, 21 critiques, synthesis and completeness check did not complete. Its workflow result was `plan: null, gaps: null`.
- Reviewed the saved proposals against `AUDIT_G3-G4_round-1_2026-09-05.md`, current package source, relevant upstream source slices, sibling patch locations and `TESTING.md`. Three independent reviewers covered lifecycle, engine/arithmetic, and redraw/test contracts. The coordinating reviewer covered documentation, verification procedure and cross-group conflicts.
- The authority excerpts quoted in the audit and proposals were treated as evidence of the existing contract. `DESIGN.md` and `DESIGN-HISTORY.md` were not opened. An implementation packet must supply any additional normative excerpts it needs.
- Branch: `program-graphics/stage-g0`. HEAD: `840fe1c92633467a63605d5b193e07eea7d92546`. The working tree already contains edits and untracked reports. It is not the clean audit tip.
- The sentinel is already restored in the working copy at `packages/program-graphics/items.c:4809`. The diff against HEAD restores `itemToBeCoded` and `"Last item"`. Preserve it; runtime verification is still required. Other inspected repair sites remain present.
- No firmware, test source, generated package output or existing documents were changed by this review. No builds, runtime probes, mutations or screenshots were run. Historical green results and proposed failure messages are not current verification.

Source snapshots at review:

| File | SHA-256 |
| --- | --- |
| `packages/program-graphics/pgmGraphics.c` | `fbe05a864c19050a314816b736b09cbbbe7b28dace34b7391d8910c050feeff0` |
| `packages/program-graphics/items.c` | `bed7ed25cbb82b625267f135e1e1b28b5875649e33c291078d86120526bf2ba8` |
| `packages/program-graphics/build-test.sh` | `2489729f46de2666538d4b8e191d6b4177b94b715e36828070381c53d4eb6a5c` |

## Disposition of all 18 findings

ACCEPT means the proposed repair shape is suitable for implementation with its stated verification. AMEND means the saved proposal must change as specified. Neither means runtime acceptance.

| Finding | Verdict | Required disposition |
| --- | --- | --- |
| G34R1-1 sentinel | ACCEPT shape; already edited | Preserve the restored upstream sentinel. Test the actual last array entry and package item uniqueness; do not select it by the duplicated `/* 2870 */` comment. Verify simulator startup after regeneration. |
| G34R1-2 backup | AMEND | Normalize/release before serialization. Cover both an open canvas and a plot-abandoned canvas with a live block; also test old backups containing mode 21. Materialize any new upstream mirror from the package base. |
| G34R1-3 ownership | AMEND | Use calculator mode for openness. Prevent allocation outside the canvas. A lazy free on the next 3D command is insufficient by itself: save or re-entry may occur first. Cover all those transitions without changing reset's no-free contract. |
| G34R1-4 transform after clear | AMEND | Recommend home transform after ERASE/PVIEW/EXIT. Reset the current-point state even when no 3D block exists. Test each transition with newly seeded content and a live non-home state. |
| G34R1-5 failed undo save | COMPLETE missing design | Fallible engine entry; refuse before counters and sampling; handle both WIREFRAME and zoom rerun; release temporary rows. Specify retained-header behavior on refusal. |
| G34R1-6 narrow span | AMEND consolidation | Guard the finite byte scale at both volume setters and zoom-derived recorded ranges. The abbreviated proposal omitted the second source. |
| G34R1-7 grid writes after body mutation | AMEND | Reacquire and validate the current header after each sample, permanently drop obsolete mesh retention, and gate completion metadata as well as writes. Cover reruns. |
| G34R1-8 3D window conversion | AMEND | Validate finite endpoints and finite nonzero signed spans/scales for 3D only. Preserve reversed axes and 2D's wider real-number domain. Specify navigation failure behavior. |
| G34R1-9 over-cap grid | AMEND tests/docs | Recommend the quoted navigation decision: no retained content means no-op. With retained lines, navigation redraws those lines and drops the unretained mesh. Test both. |
| G34R1-10 test-driver leak | AMEND fixture | Return owned blocks before direct test resets; make the reset test own a live block. Measure balanced allocations in a controlled fixture, with explicit undo accounting. |
| G34R1-11 missing/vacuous tests | AMEND | Implement the behavioral coverage matrix below. Derive keys from the active layout. Do not treat matching test-name strings as evidence of executable coverage. Resolve name collisions centrally. |
| G34R1-12 empty redraw passes | AMEND oracle | Require independently established nonblank geometry plus still-to-first-redraw equality. Do not compare redraw only with another redraw. |
| G34R1-13 duplicated coordinates | ACCEPT test-first shape | Use an asymmetric seam test and mutate each coordinate computation separately. A helper refactor is optional and is unnecessary to close the coverage gap. |
| G34R1-14 W3 pre-lit pixel | ACCEPT with precondition | Clear using the package's ERASE gesture and assert the probe is unlit before the refused-range operation. The invalid-window mutation must then fail. |
| G34R1-15 documentation | AMEND wording/checks | Keep one limits list, correct the shift rule including SNAP, repair citations. Avoid brittle document checks with fixed item counts or assumptions that all references are local headings. |
| G34R1-16 D17b mutation record | ACCEPT correction | Retire the stale row and name restoration of the old cap guard in place of `pgGlyphBoundary`. Do not claim the mutation ran during this review. |
| G34R1-17 header size | ACCEPT | Add `_Static_assert(sizeof(pg3dHeader_t) == PG3D_HEADER_BYTES, ...)` immediately after the typedef. Independently perturb the struct and the offset macro to prove the check is compiled. |
| G34R1-18 S3 diagnostic | ACCEPT | Use one existing baseline constant for both comparison and diagnostic. This fixes a message; it does not establish rendering correctness. |

## Completed engine design and arithmetic review

### Undo-save failure: entry must have a success result

Current `pg3dEngineEnter` calls `saveForUndo()` and then changes engine counters and the solving flag (`pgmGraphics.c:1077-1087`). Its callers are WIREFRAME (`1142`) and rerun (`1302`). The relevant upstream convention is the `ERROR_RAM_FULL` check after saving undo in `src/c47/items.c:300-310`.

Make entry return success/failure. Check for RAM_FULL immediately after the save and before counters/flags change. On failure, report the existing error and do not call engine leave: there is no successful entry to unwind. WIREFRAME releases its row allocation; rerun leaves its existing recorded grid and range intact. Preserve upstream's semantics for discarded older undo storage rather than promising that every byte of total pool usage is unchanged.

WIREFRAME currently invalidates and reserves header dimensions before allocating rows (`1129-1142`). The implementation must choose and test the boundary. Recommendation: calculate capacity without destroying the old grid, then commit dimensions/invalidation after successful engine entry. This preserves old retained content on refusal. Keep the previously documented view-freeze ordering on allocation failure. This is a specific amendment requiring coverage, not a reason to move all view state.

The memory fixture must configure labels, dimensions, stack values and matrices before exhaustion. Prove row allocation succeeded and the undo save itself failed. A fixed 64x64 matrix is not a portable trigger across solo and combined builds. Prefer a narrowly scoped TESTSUITE-only failure seam at the undo-image allocation, or bounded pool exhaustion with explicit evidence of the failure site. Restore the seam and all allocations on every exit. Do not modify the allocator or `freeList.c`.

Required observations for both callers: zero sample executions; unchanged stack values and matrix contents; unchanged engine counters and solving state; correct error; no leaked temporary rows. A rerun test must first prove that its valid control input triggers rerunning. Mutate each caller's failure branch separately. Do not count an earlier allocation failure as proof of undo-save handling.

### Header consistency must control writes and completion

The sample loop checks only block existence/frozen state before writing (`pgmGraphics.c:1056-1058`); WIREFRAME's final grid-valid condition also compares dimensions (`1152`). ERASE clears those dimensions (`861-865`), and a body can freeze the header again through LINE3D.

After each program sample, reacquire the current header. Permanently stop retaining the old mesh if the block is missing, is not frozen, or either dimension differs from this run. Never dereference the stale header after a body can replace/reset its storage. Keep this in the shared sample loop so reruns receive the same protection. Communicate whether retention survived back to the caller, or use one consistent completion predicate: the outer by-value `retain` is not updated automatically.

Rerun currently writes new recorded Z bounds on `PG3D_RUN_OK` alone (`1308`). Gate that metadata update on the same surviving retained grid. A successful series of samples does not prove the old grid still exists. Do not clear legitimate LINE3D records that the body deliberately added while discarding obsolete mesh writes.

Tests: ordinary retained draw; ERASE-only body; and the audit's conditional ERASE/re-freeze body over a 17x17 grid. Prove ERASE happened once and enough line records accumulated to reach the original collision. Verify every retained endpoint through the existing layout helpers/constants. Repeat for a rerun. Mutation: restore the frozen-only write guard while leaving the final dimension guard intact. A second mutation must expose an unconditional rerun Z-range update.

### Volume ranges and display windows have different domains

Volume intervals remain strictly increasing. Validate their positive finite span and the finiteness of `254.0f / span`, using the project's numeric idiom. Apply the same predicate to the zoom-derived recorded interval (`pg3dZoomRerun:1325-1335`). Clipping a valid wide interval can produce an unusably thin interval: the saved fallback argument to the contrary is false.

Test all three volume setters on both sides of the finite-scale boundary, verify encoding at interior points and preserve prior state on refusal. The saved 7.4e-37/7.6e-37 inputs are candidate boundary fixtures, not measurements from this review. Separately reach a thin zoom interval from a valid volume and mutate its guard independently.

Display windows accept reversal: `pgRange:473-483` rejects equality, not negative span, and zoom uses the magnitude of its pixel scale (`1331-1332`). Rejecting negative spans/scales would break mirrored graphics. For each axis require finite converted endpoints, a finite nonzero signed span and a finite nonzero signed scale. Keep the 2D setter and its real-arithmetic mapping unchanged.

Validate window conversion before commands freeze a view or append geometry. Do not move a call that reads an uninitialized `view`; split window-field validation from view/matrix setup if needed. Handle every setup caller: WIREFRAME, LINE3D, redraw, zoom preflight and rerun. Preserve LINE3D's no-previous-point branch, which only establishes the starting point, and PT3D, which has no projection setup.

Recommendation for navigation: reject an unusable window before clearing the canvas or changing navigation state, report the domain error and preserve retained content. This needs a stated contract in the implementation packet; silent blanking is not an already-approved behavior. Test invalid X, invalid Y, both reversed axes, ordinary accepted boundaries, retained grid and lines, and continued successful 2D drawing. Prove a valid rerun control before testing an invalid-X refusal: X invalidity alone does not make the Y-based rerun threshold fire.

## Lifecycle and backup integration

Restoring the sentinel is a small repair, already present in the working area. It does not prove that restored backups are safe, or that generated output and the simulator contain the repair.

The saved backup proposal closes a live canvas before serialization, following upstream's save-time normalization for confirmation mode. However, `pgCloseView` returns when the calculator is already outside canvas mode (`pgmGraphics.c:109`). Plot abandonment can leave an owned block behind. Freeing it only on the next 3D command misses a user who quits immediately or opens another canvas first.

Separate resource disposal from screen/mode restoration. Use the existing block-release mechanism for owned graphics storage even outside canvas mode; apply the ordinary UI close only to a genuinely open canvas. Run save cleanup at a point after save cancellation has been resolved but before the pool/mode is serialized. Keep reset's existing no-free rule: global reset owns pool destruction, and the test must not disguise it with a direct reset that loses a live allocation. Test save/cancel, plot-abandon/save, plot-abandon/re-entry, and a backup made by the previous version with mode 21. If accepting old backups needs restore-side normalization, place its hunk away from both forth-core and undo-history changes and test the combined stack.

A new `saveRestoreBackup.c` override must be explicitly named in the implementation packet and created with `python3 tools/pkg_patch_refresh.py packages/program-graphics --materialize saveRestoreBackup.c`. The saved instruction to copy the current upstream file is rejected. Generated `patches/` and `files/` are never hand-edited. Row relocation is a separate compatibility decision; it is not necessary to restore the sentinel and is excluded from this repair wave.

For clear semantics, recommend home transform on ERASE/PVIEW/EXIT. `pg3dEmpty` returns before clearing `haveCur` when there is no block (`862`); an outside-view PT3D can therefore survive first entry. Clear the current-point state and selected transform independently of block presence. Pin outside PT3D → PVIEW → first LINE3D: it should establish a point without a phantom segment.

Do not reuse the proposed transform fixture unchanged. It clears content with PVIEW and then tries UP without drawing again; navigation correctly returns on empty content. Reseed each ERASE/PVIEW/EXIT subcase, assert non-home state before the clear, then compare with a fresh home canvas. No fixed 798-pixel oracle is required.

## Regression tests that can actually disprove the fix

Give new tests descriptive IDs prefixed `G34R1-`, leaving existing names and a mapping for historical references. The saved groups independently allocate P30/P31, and existing P27/P28 already have conflicting meanings. One consolidated manifest must identify each executable test, required behavior and mutation; a comment or `printf` containing a name is not coverage.

| Behavior | State/observation required | Mutation that must be detected |
| --- | --- | --- |
| Real navigation | Derive the active key mapping, exercise press/release/click as applicable, prove shift state and dispatched item. Cover f/g with both UP and DOWN and ordinary rotation/zoom/home. | Remove shift admission or alter one translation/release route. |
| STOP abort | Reach sample execution, deliver the actual abort path, prove fewer than the planned samples ran and engine/user state is restored. | Remove abort propagation/check. |
| Nested WIREFRAME | A running body dispatches a nested command; assert refusal, bounded sample count and preserved outer state. | Bypass nesting refusal with bounded execution safeguards. |
| Pointer restoration | Enter with a real loaded program/current step, invoke through item dispatch, and verify return program, local step and pointer plus surrounding stack state. | Remove one restoration assignment at a time. |
| Allocation refusal | Complete setup first, force the intended allocation failure, verify the failure site and return state. Distinguish retained-block, row and undo-image failures. | Remove the corresponding guard; stop safely on corruption/crash. |
| Frozen eye | Retain a valid picture, change live eye/volume, show it remains frozen until the chosen clear transition, then show new settings take effect. | Use live parameters instead of recorded parameters. |
| Redraw content | Independently establish a nonblank asymmetric scene; compare still canvas with the first home redraw, including mixed retained lines/grid. | Blank redraw; change X divisor; change Y divisor, separately. |
| Partial/all holes | A real program creates a known finite/hole mask. Observe no segment crossing a hole and preservation of valid adjacent segments; separately assert all-hole error. | Remove row/column hole checks independently. |
| Over-cap behavior | Draw an over-cap mesh alone and with retained lines; assert header state, unchanged canvas/angles for the empty-retention case, and line-only redraw in the other. | Bypass empty-retention return; do not force an overflowing allocation. |
| Pool balance | Own the live block in the fixture; compare like-for-like states after freeing graphics, rows, fixture programs/registers and normalizing undo ownership. Repeat the driver. | Remove each required release separately. |
| Refused window | Assert the chosen probe is clear first, reject a range, then draw through the preserved window and observe the expected geometry. | Commit invalid range fields before refusing. |

The active keyboard is model-dependent. R47 indices 22/10/11 are not universal: the C47 layout uses different navigation positions and a combined f/g key. Derive indices from the selected table or explicitly select and restore each supported layout. An unreachable fixture must stop the subcase and clean up; it must not continue clicking guessed keys.

The proposed inverted-mode oracle assumes the same number of lit pixels as set mode. Shared vertices/edges can cancel under inversion. Use an independent geometric expected mask or a composition property with a nonblank control, scoped to the canvas. Avoid font/header pixels and additive pixel-count arithmetic when edges overlap. Existing showcase counts may remain contextual same-build regression observations; they are not new universal geometry contracts. Re-run and frame counts must be read from the actual fixture, not assumed unchanged after adding tests.

Do not defer partial-hole behavior merely because the old proposal lacks an oracle. A real body can produce valid values in one column and holes in another. Assert the recorded hole mask and compare with only the known valid adjacent segment(s), using explicit endpoints or independently projected geometry. Use an asymmetric fixture and both row/column cases. All-holes is not a replacement.

## Documentation and verification procedure corrections

The README currently says shifted keys do nothing (`README.md:224`) while its table lists rotations (`76-77`). The proposed replacement also incorrectly says every other shifted key does nothing, omitting SNAP. State that f/g-UP/DOWN rotate, SNAP remains available, and other shifted commands including f-EXIT/OFF are swallowed as currently implemented (`keyboard.c:2783-2805`). Put the complete rule in the implementation packet's quoted authority update.

Keep section 10 as the proposed canonical limits list and retain section 9.9 as a pointer. Repair dead citations without renumbering unrelated limits. The suggested generic citation checker must distinguish external-document references, headings, list items and line citations; otherwise it rejects valid prose. Fixed totals such as exactly 26 limits, a fixed patch-hunk count or a fixed suite assertion count are not durable checks. A targeted editorial pass is sufficient for simple text changes.

`TESTING.md:70,83` names an obsolete D17b mutation. Current `pgGlyphBoundary` is at `pgmGraphics.c:647-653`, the cut at `669`, and the canary at `1982-1987`. Correct the record to the old cap-guard mutation; retain its actual execution status. Token presence alone cannot prove a mutation exercises the asserted behavior. Add the header-size assertion after `790`, following the existing `_Static_assert` convention in `packages/forth-core/forth_prims.c:295`. For S3 (`2432`), share the existing baseline value between assertion and message.

The saved verification protocol also needs these corrections:

1. Baseline the current dirty working area and preserve all pre-existing work. Do not demand an empty `git status`, restore generated upstream files blindly, or commit between steps without authorization. Recover mutations by precise inverse edits in an isolated implementation context, not destructive Git commands.
2. The supplied project instructions sanction only `./packages/forth-core/build-test.sh`. The existing program-graphics runner has its own solo/combined checks (`build-test.sh:46-99`), whereas `TESTING.md:25-35` still lists older `make pkg_build` commands. The implementation packet must reconcile the gate scope before running program-graphics builds. This review ran none. Do not copy the proposal's handwritten meson/ninja, `make dmcp5r47`, ASAN or GUI commands as authorization.
3. When sanctioned, require the package runner's status and success banners in both configurations; additionally run the forth-core gate for the combined package change. Merely defining `FORTH_DEBUG_SELFTEST` is not evidence its separate self-test battery executed: the program-graphics runner calls `meson test ... testSuite` only.
4. Record the exact revised source/generated snapshot for each run. Regenerate through the sanctioned runner before validation. Inspect compiler warnings from the complete real build log; a second incremental ninja piped to grep can report zero warnings because it rebuilt nothing.
5. Separate repair evidence (new test fails before repair, passes after, fails under the named mutation) from new-coverage evidence (existing behavior passes, known bad mutation fails only after the new test). Compile-time invariants have compile-failure evidence. Documentation/message repairs do not need contrived full firmware mutation cycles.
6. Do not declare every later equation-suite failure to be the historical upstream parser over-read. Preserve the suite order with `program_graphics` after `integrate_cov` (`testSuiteList.txt:455-457`), then diagnose each failure. The old cause is a candidate, not proof. No upstream report is sent as part of this review.
7. Verify startup and backup round-trip with the approved simulator recipe, not an exit status alone. Capture the relevant mode, resource ownership and error outcome. Image comparisons should use the stable canvas, since time/date headers can vary. No LCD capture is authorized by this review alone.
8. Use measured pool ownership, not the fallback's arbitrary 64-block leak allowance. A fixed 800-entry exhaustion array also needs a bound or a checked graceful failure; greedy chunk remnants can exceed the proposal's estimate.
9. Preserve 2D command-path cost; inspect the final diff for per-step changes. Compare timings under matching build/load conditions; do not make old wall-clock values hard acceptance limits. Device size/RAM deltas remain unverified until an authorized target build measures them.

## Proposed implementation order and decisions

1. Confirm the package gate/capture scope, source baseline and contract excerpts in an implementation packet. Preserve the already-restored sentinel and regenerate/test it with the item invariant and startup check.
2. Repair the leaking test fixtures and W3; add the compile-time invariant and correct the S3 message. Establish stable ownership and rendering fixtures before memory-pressure tests.
3. Implement engine refusal and grid-write/completion consistency together with their distinct initial-run/rerun tests. Then implement both numeric guards, retaining reversed windows.
4. Implement coordinated close/abandon/save/re-entry cleanup and the chosen clear-transform semantics. Test legacy backups and cancellation as separate cases.
5. Complete real-key, abort/nesting/restoration, frozen-view, partial-hole, redraw-seam and over-cap coverage. Execute each named mutation once it can fail for the intended reason. Integrate the full suite and document observed results.
6. Consolidate the authority/README/testing records and perform a focused review of the actual repair diff. This design review cannot substitute for checking the implementation that is eventually written.

The implementation packet should state these behavior choices explicitly:

| Choice | Recommendation | Alternative/tradeoff |
| --- | --- | --- |
| ERASE/PVIEW/EXIT transform | Return to home and clear current point. | Retaining transforms requires still-picture zoom/resampling semantics and additional tests. |
| Unsupported 3D float window | Visible refusal with retained picture/state preserved; keep reversed finite windows. | A documented limitation or silent blank is possible, but offers weaker behavior and must be stated. |
| No retained content after over-cap draw | Navigation is a no-op, consistent with the quoted key decision. | Clearing on the next key changes that decision and requires code plus revised tests. |
| Undo failure/header state | Preserve old retained grid; refuse sampling. | Invalidate/clear explicitly on failure, if chosen and documented. Never leave unexplained dimensions reserved. |
| Backup compatibility | Normalize new saves and safely close old mode-21 restores; release orphan resources. | Save-only repair leaves old backups unresolved and needs an explicit compatibility limit. |

Memory refusal, valid grid-write ownership, preserving mirrored windows and correcting false test oracles are correctness requirements, not optional documentation cleanups. No item relocation, broad state rewrite, independent upstream parser repair, commit or external publication is part of this plan.

Completion means the missing proposal has been supplied, all 18 findings have a disposition, and the three critique lenses have been consolidated. Runtime acceptance remains pending implementation and the measured checks above.
