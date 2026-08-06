# forth-core — DESIGN AUDIT

A repeatable check that the package still obeys its own design philosophy.
Not a bug hunt: `CODE_AUDIT.md` (the adversarial multi-reader audit),
`/code-review` and the R-series do that. This asks a different question —
**is this still the design we decided on, or has it drifted?**

Run it at every stage close, before any release/MR, and after any session that
touched an upstream file. It is deliberately short enough to run often.

```bash
./design-docs/forth-core/design-audit.sh
```

That covers Part 1. Parts 2 and 3 are judgement and cannot be scripted; Part 3
is the one that pays for the whole exercise.

---

## How to use the findings

A finding is a **prompt to think**, never proof of a defect. Every item below
has a legitimate answer as well as a guilty one. The audit fails only if you
cannot say which.

Three outcomes per finding, and all three are fine:

- **Fix** — the drift is real; correct it.
- **Accept with reason** — record it in `DESIGN-HISTORY.md` and, if it moves a
  count, re-baseline with `--accept` and say why in the commit.
- **Defer with a name** — it is real but blocked on something. Name the blocker
  (an upstream MR, an owner ruling). "Later" is not a name.

The one thing you may not do is leave a finding unexamined because the numbers
looked familiar.

---

## Part 1 — mechanical (scripted)

`design-audit.sh` splits its checks by how tolerant they are.

**HARD — never acceptable, always a finding:**

| Check | What it means when it fires |
|---|---|
| **A. footprint over budget** | A *new* upstream file is being touched, or the patch set is growing. Every upstream file this package touches is a permanent rebase liability. |
| **C. no-op churn** | Trailing whitespace or blank-line edits that change nothing but conflict forever. Revert to upstream's exact bytes. Found and cleared in S1; re-appeared in S3 (three reformatted `keyboard.c` guards) and was caught by this check. |
| **F. generated drift** | `patches/`/`files/` disagree with the working area, or were hand-edited. They are build output. Run refresh. |
| **G. stray shippable content** | A non-source working-area file has no `.pkgignore` entry, so refresh will copy it into `files/`, shadow it into the build tree, and ship it inside the firmware. This check caught `design-audit.sh` itself. |

**REVIEW — legitimate in bounded amounts; flagged only when the count grows
past `.design-audit-baseline`:**

| Check | What it means when it grows |
|---|---|
| **B. hunks with no Forth content** | The strongest single signal that something not ours is riding along. This is exactly how S1 found the global-register descriptor memset. Accepted today: `core/freeList.c` (deferred, see below), a masked-low-byte comment in `manage.c`, and `testInitVariableSoftmenu` in `softmenus.c`. |
| **D. contiguous added blocks ≥ 12 lines** | Package logic is being written *into* an upstream file instead of a package-owned `.c`. This is what S2 unwound. `manage.c` is the standing exception — see Part 2.1. |
| **E. package-owned allocations** | Listed every run, never auto-flagged. Each one needs an answer to Part 2.5. |

| **H. DESIGN.md citations** | A `[VERIFIED: path:line]` citation points at a file that no longer exists, so the authoritative document is describing code that is gone. Added after the first audit, which is how it was found that DESIGN.md still cited `error.c` and still claimed the dropped error-text extension was live, one stage after S1 removed both. |

Re-baseline only deliberately:

```bash
./design-docs/forth-core/design-audit.sh --accept
```

**The baseline suppresses growth alarms, not the obligation to justify what is
already there.** On a first run, after any `--accept`, and every few audits
regardless, read the REVIEW lists on their merits rather than checking that the
counts match. The counts matching means nothing got worse; it does not mean
what is there is right.

**Run check D *before* planning a move-out stage, not only after.** S2's whole
purpose was moving package logic out of upstream files, and it missed the
single largest instance — ~129 lines of capture orchestrators in `manage.c` —
because it worked from hunks already catalogued by hand instead of enumerating
them. Check D lists them in one command. Use the mechanical half as an input to
planning, not just as verification afterwards.

---

## Part 2 — philosophy conformance (judgement)

One question each. Answer them against the *current* tree, not from memory.

### 2.1 Is upstream coupling still minimal, and is what remains structural?

Upstream files are touched only where the change genuinely cannot live in a
package file. The test is: *if this were a call to package-owned code, would
anything be lost?* If not, it belongs in a package `.c`.

The standing exception is `programming/manage.c`. Forth's changes there
concentrate in `pemAlpha` and `insertStepInProgram` — a new PEM submode woven
through a state machine, not a call-out at a seam. That is accepted, and it is
why "near-zero upstream overrides" is not a goal for this package. Every
*other* file should be trending toward a call site.

A hooks package was explored and abandoned (2026-07-25): it would have raised
total files touched, and would not have helped `manage.c` at all.

### 2.2 Is state still derived, not stored?

The package's rule is that entry state comes from the program bytes at the
cursor and is never persisted — `forthEntryStateAtCursor`,
`forthEntryStateAtInsertion`, `forthMarkerTurnsOn`. A new `static` or global
that caches something the program bytes already say is a violation.

The honest counter-example, and worth remembering before assuming everything
is derivable: the capture's OPEN/SUSPENDED flag **cannot** be derived from
`calcMode`/`FLAG_ALPHA`/`tam.function`, because `tamEnterMode` overwrites
`tam.function` before the suspend seam fires. Deriving is the default; when it
does not work, say why at the declaration, as `forth_capture.h` does.

### 2.3 Do names still persist, never indices?

DESIGN.md §4.2: a program step records the NAME. Dictionary indices are not
stable across edits. Every path that records a Forth call in PEM must go
through `insertUserItemInProgram`; only live execution uses
`reallyRunFunction(ITM_FCALL, widx)`.

```bash
grep -rn "ITM_FCALL" packages/forth-core --include=*.c | grep -v test_dict_reloc
```

Every hit should be a live-execution path or the shared
`forthDispatchColon()`. A new `ITM_FCALL` on a `CM_PEM` branch is a defect —
this was code audit #3, at three sites.

### 2.4 Is the program step still the single source of truth?

Capture text lives in `aimBuffer` transiently; the committed step is
authoritative and is re-committed after every keystroke. Anything that writes
the buffer without re-committing breaks the invariant that made power-off
lossless — that gap is precisely what code audit #1 fixed, and what
`forthCaptureSuspend()`'s unconditional recommit now defends.

### 2.5 Does every package allocation outlive a save/restore cycle?

`restoreCalc()` restores allocator bookkeeping wholesale from the backup file.
An allocation whose lifetime is *shorter* than a save/restore cycle is
therefore reintroduced as a phantom entry nothing will free
(`UPSTREAM_REPORTS_976b864b5.md`).

Check E lists the candidates. Today only `forth_dict.c` allocates: `gdict` is
persisted (lifetime ≥ the cycle, fine) and `fdict` is per-lifetime but reset
at the documented seams. S3 removed the one violating case. **Any new
short-lived allocation reopens this leak** — prefer an existing native buffer,
as the capture now does.

### 2.6 Are upstream bugs reported rather than locally fixed?

Standing owner ruling (FIX-6 precedent): a defect in upstream code gets a
write-up in `UPSTREAM_REPORTS_*.md`, not a local patch, unless forth-core
cannot function without the fix. A local fix to generic upstream behaviour is
a check-B finding waiting to happen.

Open: `UPSTREAM_REPORTS_globalRegister_reset.md`,
`UPSTREAM_REPORTS_976b864b5.md`, `UPSTREAM_REPORTS_b8f79e486.md` §3 (FIX-6B
agreed, unlanded — which is why `core/freeList.c` is still in the package).

### 2.7 Are the tests still toothed?

The suite is the only thing standing between a refactor and silent breakage.
Spot-check, do not re-audit wholesale:

- Does any assertion re-pin a contract the current design *reversed*? S3 hit
  this: `test_capture_buffer` subcase 2 asserted `aimBuffer` stays empty, the
  exact opposite of the new design.
- Does any test still guard something that can no longer happen? Keep it if it
  now guards something else, and say so — the arena-residue subcases survive
  S3 as step-churn regression guards, not capture-leak guards.
- Did the PASS count move without a test being added or removed?

### 2.8 Do DESIGN.md and the code still agree?

`DESIGN.md` is authoritative and states only what is true *now*;
`DESIGN-HISTORY.md` is the append-only amendment trail. After any stage that
changed a decision, one entry lands in the trail and `DESIGN.md` is corrected —
not annotated.

Check H automates the cheap half (do cited paths exist). The expensive half is
not automatable, so sample it: take the two or three DESIGN.md sections
covering whatever changed most recently and read them against the code.

This is worth real suspicion. The first audit found that DESIGN.md described
the capture as living in `aimBuffer` throughout — correct before F6-1, wrong
for the entire F6 series after it moved the text to a managed buffer, and
accidentally correct again once S3 moved it back. **The authoritative document
silently disagreed with the code for a whole stage series and nothing caught
it.** A citation check would not have found that one; only reading would.

A useful tell: a claim written as `[VERIFIED: ...]` is evidence about the tree
*at the time it was written*, not a standing guarantee. Treat an old VERIFIED
tag on recently-changed code as unverified.

### 2.9 Are the measurements recorded?

Binding per CLAUDE.md: report arena high-water with any dictionary change, and
record the measured flash delta in the stage commit.

```bash
make dmcp5r47 CUSTOM_PKG=packages/forth-core CUSTOM_PKG_RECONFIGURE=1 2>&1 | grep '^flash'
```

`CUSTOM_PKG_RECONFIGURE=1` is not optional. `build.dmcp5`'s stamp tracks only
the `CUSTOM_PKG` *value*, not package content, so a measurement without it
silently reads a stale shadow and reports the previous tree's size.

### 2.10 Is the design idiomatic, or fragmented?

Owner rule (2026-08-02): **a design must be idiomatic — it must not cause
confusion or deviations because it is fragmented.** One concept gets one
rule; a namespace does not carry two implicit conventions; a mechanism that
works one way in one place must not work a subtly different way in another.
When extending an existing mechanism, the extension either follows the
existing idiom exactly or replaces the idiom wholesale — a special case
bolted onto a general rule is the finding.

Where a deviation is nevertheless retained deliberately (compatibility,
churn cost), it must be (a) single-sourced — one function or one constant
that every consumer goes through, never re-derived at call sites, (b)
documented at its definition with the reason, and (c) recorded here with a
**named trigger** for removing it. An undocumented or multi-sourced
deviation is always a finding.

Standing accepted instance: the working-area path mapping. Bare rels mean
`src/c47/<rel>` (implicit root) while a `SIBLING_ROOTS` first segment means
`src/<rel>` (explicit root) — two rules in one namespace, accepted 2026-08-02
because the uniform alternative (rooting the working area at `src/`, i.e.
`c47/keyboard.c`) costs a full package rename, regenerating every patch
under new names, and rewriting every `[VERIFIED: packages/forth-core/...]`
citation in DESIGN.md. Single-sourced in `upstream_repo_rel()`
(`tools/pkg_patch_common.py`); documented in PACKAGE-MANAGER.md. **Trigger:
a second sibling root.** The moment one is proposed, do not extend the
whitelist — do the uniform `src/` mirror refactor instead.

---

## Part 3 — the expired-premise sweep

**This is the part worth doing.** Everything above catches things that were
always wrong. This catches things that were *right when built* and quietly
stopped being right — the failure mode a frequently-run audit exists for,
because no single commit ever looks wrong.

The canonical case: the separate Forth capture buffer. F6-1 introduced it for a
real, correct reason — TAM-cancel zeroed `aimBuffer` and destroyed a suspended
line. F6-2 then made the on-disk step the single source of truth, which made
the text always recoverable and dissolved the reason. Nobody went back. It cost
13 ternaries across five upstream files, an allocator lifetime, and a genuine
save/restore leak, for about a year of stage work, until S3 removed it.

Pick **two or three** mechanisms per audit — rotate, do not attempt all —
and for each ask:

1. **What was this built to prevent?** Find the DESIGN-HISTORY entry or the
   commit. If you cannot state the original reason in one sentence, that is the
   finding.
2. **Can that thing still happen?** Re-derive it against today's code. Do not
   trust the comment; comments outlive their premises, which is the whole point.
3. **If it cannot — what is this still buying?** Sometimes a second reason
   accrued and it should stay, documented. Often nothing, and it should go.
4. **What does it cost?** Call sites, state, allocations, upstream hunks. Cheap
   vestiges can wait; expensive ones are the next stage.

Rotation list — mechanisms with a stated premise worth re-testing:

- the capture state object (does it still need three states?)
- `forthCaptureSanitizeRestoredUi` (needed only while capture state is
  process-local and unpersisted)
- the `doFnReset` hook reorder (premise narrowed once in S3 already)
- `core/freeList.c`'s guard (premise is FIX-6B landing upstream)
- `param_core.c`'s bounded reader (would over-allocating the synthetic buffer
  remove the need to thread `end` at all?)
- the `_closeCatalog` selftest export and the `config.c` selftest trigger
  (both reviewed and kept in S2 — re-test when the harness changes)
- `forthPickerGuard`'s menu-identity check
- the 256-byte / 196-glyph capture cap, now that the sink is 1024 bytes

A mechanism that survives the sweep should get a one-line note in
`DESIGN-HISTORY.md` saying it was re-tested and why it stays. That is what
stops the next audit re-deriving it from scratch.

**When a premise expires, fix the comment even if you keep the code.** A stale
justification is worse than none: it will be believed. The first audit found
two of these on the same mechanism — `forthCaptureSanitizeRestoredUi` still
described repairing offsets into a buffer that no longer exists, and the
`doFnReset` reorder claimed to be preventing rejected frees by functions that
do not free at all. The second was written *during S3, by the person correcting
the first*, which is the lesson: a rewritten justification is a new claim and
needs checking like any other, not inheriting the credibility of the comment it
replaced.

---

## Reporting

Append to `DESIGN-HISTORY.md` under a dated heading. Keep it short — the
audit's value is the cadence, not the paperwork.

```markdown
## <date> — design audit

Mechanical: CLEAN | <n> findings (<one line each: fix / accept+reason / defer+blocker>)
Philosophy: <only the questions whose answer changed>
Expired-premise sweep: <mechanisms examined> -> <verdict each>
Footprint: <files> files, <n> added lines, flash <n> (<delta>)
Actions: <what landed now> / <what is queued, with a name on the blocker>
```

If three consecutive audits are clean and the sweep finds nothing, lengthen the
interval — an audit nobody believes in gets skipped, which is worse than one
that runs quarterly and means something.
