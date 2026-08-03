# forth-core — TESTING.md (the two harnesses and their reconciliation)

Owner ruling (2026-08-02): the F6 stage-exit **hardware** bench (runbook row
11i) converts to an **automated sim-run check** — the sim catches most of the
bug classes the bench targeted and can run on every gate. The prerequisite is
reconciling the package's testing framework with the one upstream uses. This
document records how both harnesses actually work (so the knowledge survives),
the evidence gathered on 2026-08-02, and the staged plan. Posting the forum
material stays an owner option, not a queue item.

---

## 1. The package harness — in-sim self-test battery

**What.** `packages/forth-core/test_dict_reloc.c` — 173 checks: dictionary
relocation/persistence, capture seams, parameter parity, stack semantics
(D1/D2 pins), the F1.5 §8.9 acceptance battery, and the F6 capture battery.

**How it is compiled in.** Only when `FORTH_DEBUG_SELFTEST` is defined. Under
the patch-based overlay the resolver never reads a package `meson.build`, so
the `meson_options.txt` option is consumed by nothing — the define is injected
by the gate script via `meson configure -Dc_args=-DFORTH_DEBUG_SELFTEST`
(`meson setup --reconfigure -Dc_args=...` does **not** reliably apply to an
existing build dir). Without the injection the suite silently compiles out and
the gate is vacuous green.

**How it runs.** The `010-config.c.patch` hook in the `doFnReset` path:
`PC_BUILD && FORTH_DEBUG_SELFTEST && headlessMode`, guarded run-once (tests
call `restoreCalc` → `doFnReset`, which would recurse into the suite), calls
`forthDictSelfTest()` and `exit(0)`/`exit(1)`. The binary is the real GTK sim
(`build.sim/src/c47-gtk/c47`) launched `--headless`.

**The gate.** `./packages/forth-core/build-test.sh` — canonical, never
re-derive the meson/ninja flags by hand:

1. `tools/pkg_patch_refresh.py` — the resolver builds the shadow tree from
   **generated** `patches/` + `files/` and never reads the flat working area;
   an unrefreshed edit is invisible to the compiler and the gate would pass
   for code it did not build (verified by marker injection).
2. `meson setup --reconfigure` (re-shadows), then the `c_args` injection
   above, then `ninja`.
3. Run `--headless`; green requires exit 0 **and** the exact banner
   `FORTH SELF-TEST: ALL PASSED` **and** `==> BUILD + SELF-TEST GREEN.` —
   status and banner together, so a wrong-binary run cannot pass.

**Disciplines that make the suite mean something** (binding for any future
harness work):

- **Mutation pins.** Every packet lands with named mutations that MUST go
  red, run separately, gate green between restores. A test that stays green
  under every named mutation is decoration — the 2026-07-21 test-suite audit
  (`cbd285e09`) removed 14 such cases and this is the check that keeps them
  out.
- **Structural fixtures.** The PROGRAM-FIXTURE AUTHORING RULE
  (QWEN_RUNBOOK §4): programs are built with `testProg_t`/`tp*` helpers and
  role-addressed step handles; never `beginOfProgramMemory + <literal>` or
  payload-length arithmetic.
- **Blast radius by PASS-set diff**, not by reading failures: keep the
  pre-gate log, `diff` the sorted `PASS:` lines.
- **Measurements.** Arena high-water with any dictionary change; flash via
  `make dmcp5r47 CUSTOM_PKG=packages/forth-core CUSTOM_PKG_RECONFIGURE=1`
  (the build stamp tracks the *variable*, not package content — without
  `CUSTOM_PKG_RECONFIGURE=1` the number is the previous tree's).

## 2. The upstream harness — `src/testSuite/`

A separate **native** executable (`build_by_default: false`): all of
`c47_src` + decNumber recompiled with `-DTESTSUITE_BUILD` plus HAL stubs
(`hal/{audio,gui,lcd,io,print_ir}.c`) and the 4.7k-line driver
`testSuite.c`. Two test styles:

- **Script-driven:** `tests/testSuiteList.txt` names the case files
  (`tests/*.txt`); each file sets machine state (`In: FL_CPXRES=0 SD=0 ...`),
  names a function (`Func: fnAdd`), and lists register expectations, compared
  to 34 significant digits.
- **C coverage hooks:** `cov*` functions in `testSuite.c` (program flow,
  backup round-trip, solver/integrator paths...) for behavior the line
  format cannot express — the precedent for anything Forth would add there.

Registered as a meson `test()` (workdir repo root, timeout 800 s):
`meson test -C build.sim testSuite`.

**Overlay interaction.** The package shadow replaces `src/c47` sources for
*every* target, so testSuite's `c47_src` compile picks up the package —
including, in a build dir configured by the gate, `FORTH_DEBUG_SELFTEST`
(harmless there: the suite's trigger needs `headlessMode`, which is the sim
binary's flag). The overlay mirror root is `src/c47/` plus the sibling
roots in `SIBLING_ROOTS` (`pkg_patch_common.py`) — since T2-A
(2026-08-02) a `testSuite/…` rel patches `src/testSuite/…`, shadowed and
compiled exactly like c47 sources (see T2 below). Meson `test()`
registration remains outside the package's reach.

## 3. Evidence (2026-08-02, base 44dc5a705)

- `ninja -C build.sim src/testSuite/testSuite` under
  `CUSTOM_PKG=packages/forth-core`: **links and builds green** (exit 0). The
  2026-07-27 integrate-worktree link failure does not reproduce on the
  current tree; treat it as environment-specific until it recurs.
- First `meson test -C build.sim testSuite` against the package-patched
  core: **GREEN — 12,071 tests passed, 0 failed, 93.5 s.** The overlay
  currently causes zero native regressions visible to upstream's suite,
  and T1 costs nothing but the recompile.
- Flash at this base: 1,094,832 B (+8 B vs the D1/D2 measurement) — the
  optimizer eliminates the dead upstream `_executeOp` block retained by the
  rebase.

## 4. Why reconcile

The 2026-07-25 parity ruling — anything that behaves differently from R47 is
a bug — is *exactly what upstream's suite pins*. Running testSuite under the
overlay turns every package-induced native regression into a red test the
forth battery cannot see (it only looks at Forth). Conversely the forth
battery pins what upstream's line format cannot express (relocation, capture
seams, PEM key flows). The two are complementary; the reconciliation is about
one entry point and shared coverage, not about replacing either.

## 5. The plan

**T1 — one entry point (packet-sized, no upstream diff).** Extend
`build-test.sh` to also run `meson test -C build.sim testSuite` after the
forth battery and require both green. The gate stays the single writer of
build state; first run pays the TESTSUITE_BUILD recompile, cached after.
Gate additions: require meson's `Ok:` summary line for testSuite, same
status+banner double-check as the forth battery.

**T2 — coverage boundary: DECIDED and IMPLEMENTED (owner ruling
2026-08-02, option 1).** The overlay now recognizes **sibling upstream
roots**: a working-area rel whose first segment is in `SIBLING_ROOTS`
(`tools/pkg_patch_common.py` — today only `testSuite`) maps to `src/<rel>`
instead of `src/c47/<rel>`. All commands apply the mapping (refresh,
materialize, rebase/integrate preflights, audit, status); the resolver
shadows an active sibling root exactly like `src/c47` and emits `SIBSRC:`
lines that the root `meson.build` turns into `custom_pkg_testSuite_src`,
which `src/testSuite/meson.build` consumes in place of its `files()` list
(fork-infrastructure hunk, same pattern as the existing `c47_src`
override). An untouched sibling root costs nothing — verified by test.
Mapping/classification/shadow behavior is pinned by 10 dedicated cases in
the tooling suites (224 total green). The package can now carry a
`testSuite.c` patch adding a `cov*`-style hook when T3 produces cases that
need it — the *capability* exists; the hook itself is deliberately NOT
pre-built (DESIGN_AUDIT Part 3 discipline).

The discarded alternatives, for the record: an upstream MR adding a
permanent package-test hook (cleaner long-term, blocked on upstream
cadence — can still be proposed later and would let the sibling patch
retire), and keeping the suites disjoint (rejected by owner: parity cases
should be expressible in the upstream driver).

**T3 — the 11i sim bench (replaces the hardware bench).** Convert
`F6_KEYBOARD_PEM_AUDIT.md` §3 Blocks A–F from hardware experiments into
self-test subcases that drive the real key paths headlessly — `pemAlpha`,
`fnKeyEnter`/`fnKeyExit`/`fnKeyBackspace`, TAM entry/suspend, catalog open —
the same in-sim key-flow precedent the F6-6 acceptance battery established.
One subcase per block row, each with a mutation pin. Rows that genuinely
require hardware (physical key bounce, DMCP power-off path, display
persistence across battery pull) are marked **HARDWARE-ONLY** in the audit
file and leave the design-binding queue — best-effort on device, mirroring
the DM42 stance. §5's exit criteria are met when every block row is either a
green sim subcase or explicitly HARDWARE-ONLY with a reason.

**T4 — packetize.** T1 and T3 are Qwen-packet-sized once T3's subcase list
is derived from the audit file; author packets per the standing rules
(QWEN_RUNBOOK §4) after the T2 decision is recorded. The runbook carries the
queue; this file carries the why.

**T5 — test-corpus restructure (RULED 2026-08-03; items 1-2 LANDED same
day — T5-1 include-part split, T5-2 accessors; item 3 queued).**
Landed deviation, recorded per DESIGN_AUDIT §2.10: item 1 shipped as an
INCLUDE-PART split (`test_capture.part.h` / `test_params.part.h`,
#included once at the end of `test_dict_reloc.c`) rather than separate
TUs — same edit-hotspot/navigation win, zero extern surgery, build/audit/
citations untouched; the full multi-TU split remains available if compile
parallelism ever matters. The core-area split (113 remaining functions)
and the tabular migration are the queued remainder. Original proposal:
Evidence, 2026-08-03: `test_dict_reloc.c` is 22.6k lines and drew 41% of
all editor failures across 327 archived implementer sessions (low-entropy
anchors on repeated scaffolds); it still carries 99 legacy
`beginOfProgramMemory + N` hand offsets — the pre-fixture-rule pattern —
and the mutation phase of the sim bench caught one weak assert (SB-C2's
form-insensitive scan) that better structure would have made obvious.
The stage, if ruled:

1. **Split the battery into per-area TUs** — `test_dict.c`,
   `test_capture.c`, `test_params.c`, `test_bench.c`, plus one shared
   `test_common.h` for the `tp*` fixtures and drive idioms. The overlay's
   `files/` mechanism already compiles multiple new sources; the suite
   runner keeps one `forthDictSelfTest()` entry calling per-area suites.
   Mechanical, packet-able, kills the megafile hotspot.
2. **Reader-side accessor API + fixture-rule extension.** Promote
   step-INSPECTION helpers (`tpSrcPayload` exists, unused) so tests never
   hand-walk `findNextStep` chains or hand-index payload bytes, and extend
   the PROGRAM-FIXTURE RULE to cover reading: layout facts come from the
   accessor, never re-derived. Burn down the 99 legacy hand offsets
   opportunistically as tests move.
3. **Migrate the genuinely tabular subsets** (parameter parity sweeps,
   glyph/outer cases, error tables) to in-C case tables now; upstream's
   `.txt` script format later via the T2 cov hook where a case is pure
   function+state→registers. Sequence-shaped tests stay C — that is where
   this codebase's real defects live (D1/D2 came from showcase programs).

## 6. What any future change to the harness must preserve

1. A gate that can go green without compiling the change under test is the
   cardinal failure mode. Both landed instances are documented in
   `build-test.sh` (refresh trap, `c_args` trap) — keep the comments there
   and the explanation here.
2. Green = exit status **and** exact banner, per suite.
3. New behavior lands with a mutation that proves the test can fail.
4. Fixtures are structural; no hand-computed addresses.
5. Flash numbers only with `CUSTOM_PKG_RECONFIGURE=1`; arena high-water with
   any dictionary change.


**T6 — upstream-style migration pilot (RULED 2026-08-03; corrects the
hygiene batch's too-broad "tabular resolved" record).** The in-C tables
were indeed already done, but migrating cases to UPSTREAM'S OWN format
is a distinct, real item — and one class fits it exactly:
interpret-state Forth lines are pure function+state→registers cases
(`In:` X = the source STRING, `Func: fnForthOuter`, expect registers),
and upstream's .txt format already handles string registers (see
addition_string.txt). Plan:

1. Build glue: the runner reads `tests/` from the SOURCE tree at
   runtime (workdir = project root), so package-provided .txt cases
   need the test() invocation remapped — extend the resolver's SIBSRC
   protocol with a `SIBLIST:` line and the src/testSuite/meson.build
   hook with a `custom_pkg_testSuite_list` variable (same pattern as
   the source-list override).
2. Package content: `testSuite/tests/forth_interp.txt` (new file via
   the sibling root) with the pilot cases — stack arithmetic, literals,
   dup/swap/drop chains, a deep-spill line — plus a patched
   `testSuiteList.txt` adding it.
3. Gate: the forth cases then run under upstream's own runner inside
   the T1 gate step, counted in its 12k+ total.

What does NOT migrate remains what the C battery exists for: anything
needing program-step context, key flows, relocation, or capture seams.