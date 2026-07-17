# R5 package-system hardening — Qwen implementation prompts

6 tasks, strictly ordered. Each is sized for a ~100k-token context window: it
names the only files and line ranges to read, and never requires reading
DESIGN.md.

**How to use:** paste the PREAMBLE, then one task block, into a fresh Qwen
session. Do not run tasks out of order.

---

## PREAMBLE (paste at the top of every task)

You are implementing one small, fully specified task in the C47 calculator
firmware repo at /home/stan/c43. You are an implementer, not a designer: follow
the spec exactly, make zero design decisions. If an anchor (a quoted line,
function, or search string) does not match what you find, STOP immediately and
report the mismatch instead of guessing.

Rules:
1. Confirm `git branch --show-current` is `forth-core/pem-entry-fixes`. If not,
   STOP.
2. The only firmware build/test command is
   `./packages/forth-core/build-test.sh`. Success = it prints
   `FORTH SELF-TEST: ALL PASSED` and `==> BUILD + SELF-TEST GREEN.` and exits 0.
   Never invoke meson or ninja directly. Tooling tasks additionally run exactly
   `python3 tools/test_pkg_patch_refresh.py`.
3. Product edits normally go in `packages/forth-core/`; never edit `src/`.
   Tasks in this R5 series that explicitly say **tooling exemption** are allowed
   to edit the named files under `tools/` (and only those named files). The
   build reads only GENERATED `patches/`+`files/`; build-test.sh refreshes first.
   Never hand-edit `patches/`/`files/`.
4. Never touch `src/c47/core/freeList.c` or any copy. Never read DESIGN.md or
   DESIGN-HISTORY.md. Never read items.c or test_dict_reloc.c in full; read only
   the ranges listed. Use `grep -a`.
5. Match surrounding code style. Keep changes narrow. Do not broaden a task to
   adjacent cleanup, including the duplicated BP-4 test class.
6. Do not commit in Tasks R5-1 through R5-5. Task R5-6 makes the single commit.
   Never `git add -A`. Never run `git stash`, `git stash pop`, `git reset`,
   `git checkout -- <file>`, or `git restore`. If you think you need to undo
   something, STOP and report.
7. If a gate goes red on an old-contract test not named by the task, STOP and
   report before touching it. Never weaken a test to make a change pass.
8. Report what changed, exact test/gate output, and anything surprising.

**Two-attempt debugger handoff (mandatory).** This rule applies only when this
task authorizes you to fix the observed error; it never overrides an earlier
immediate-STOP rule. After a command, test, or gate first fails because of your
task changes, you may make at most two distinct repair attempts. A repair
attempt is an edit intended to clear that failure followed by rerunning the
relevant command. The original task implementation is not a repair attempt. If
the required command is still not green after repair attempt 2 — even if the
visible error changes — STOP. Do not make a third repair, broaden scope, or use
git to undo anything. Leave the tree exactly as it stands; read-only inspection
is allowed only to prepare this report:

`[SOL DEBUGGER HANDOFF]`

- task ID and exact failing command;
- original failure and its relevant verbatim output;
- attempt 1: files/hunks changed, rationale, and resulting output;
- attempt 2: files/hunks changed, rationale, and resulting output;
- current `git status --short`, `git diff --stat`, and relevant diff excerpts;
- your best remaining hypotheses and anything that surprised you.

---

## R5-1 — Canonicalize patch mode metadata

**File(s):** `tools/pkg_patch_refresh.py`,
`tools/test_pkg_patch_refresh.py`

**Tooling exemption:** this task is explicitly exempt from PREAMBLE rule 3.
Edits under `tools/` are the intended product edits. Do not edit generated
package patches by hand.

**Read:** in `tools/pkg_patch_refresh.py`, grep `def generate_patch` and read
that function only. In `tools/test_pkg_patch_refresh.py`, grep
`class TestPatchValidity` and read that class only.

**The defect.** `generate_patch` diffs the base bytes through a
`NamedTemporaryFile`, so the generated patch inherits the temporary
filesystem's executable-bit behavior. Verified with identical base/working
bytes and edits but two temp locations: an ext4 temp produced
`index ... 100644`; a `/mnt/c` temp produced `old mode 100755`,
`new mode 100644` and a different manifest hash. In the real repo a normal
refresh under the latter temp location rewrote all 14 patch headers and all 14
manifest hashes. The resolver transports file bytes and writes the final shadow
file itself; this mode metadata is not delivered to the compiler. It is
platform-dependent review/hash noise, not package content.

**The change.** Make generated patches content-only and byte-deterministic:

1. Before invoking `git diff`, compare `base_bytes` with the materialized
   file's raw bytes. Return `None` when they are equal, regardless of modes.
2. Keep `git diff --no-index --full-index -U<context>` and the 40-character
   pre-image SHA gate unchanged.
3. Canonicalize the raw diff before rewriting paths:
   - remove any `old mode ...` and `new mode ...` lines;
   - canonicalize the single `index <old>..<new>` line to
     `index <old>..<new> 100644`, replacing any existing mode suffix;
   - do not alter either blob SHA or any hunk content.
4. Keep the binary-file rejection unchanged and before any operation that
   could disguise binary output.
5. Add a short code comment: patch application materializes/writes byte
   content, so canonical `100644` metadata prevents host-temp modes from
   becoming package state.

Add tests that directly exercise both raw header shapes (one `index ...
100644`, one `old mode`/`new mode` plus a suffix-less `index`) and assert the
same canonical header. Add an end-to-end assertion that a generated patch has
no `old mode`/`new mode` line and its index line ends in `100644`. Also assert
that byte-identical content returns no patch even if the working file mode is
changed where the test filesystem supports `chmod`.

**Tests that encode the old contract.** None — modes were accidental host
state, not a supported package feature. Existing patch-validity and
git-apply tests must remain unchanged and pass.

**Facts the harness forces on you.** The pre-image SHA is a content blob SHA;
canonicalizing the mode suffix does not change it. Do not weaken the
`git cat-file -e` ancestry check.

**Gate:** first `python3 tools/test_pkg_patch_refresh.py`, then
`./packages/forth-core/build-test.sh`. The build gate is expected to refresh
generated patch headers/manifest hashes; inspect those generated diffs but do
not hand-edit them.

*Verified mutation:* changing only `tempfile.tempdir` from an ext4 directory
to the mounted Windows temp directory changed the patch from `index ...
100644` to `old mode 100755` / `new mode 100644` for the same content edit.
The patch texts compared unequal.

**Report:** paste the tooling-test summary, both firmware success lines, and
the list of generated patch/manifest paths changed by the canonicalization.

---

## R5-2 — Reject an unavailable recorded base before classification

**File(s):** `tools/pkg_patch_refresh.py`,
`tools/test_pkg_patch_refresh.py`,
`custom_package/PROPOSED_SPEC_CHANGES.md`

**Tooling exemption:** this task is explicitly exempt from PREAMBLE rule 3.
The named `tools/` files and the package-manager spec are the intended edits.

**Read:** in `tools/pkg_patch_refresh.py`, grep and read only
`def ensure_base_commit`, `def base_file_content`, `def materialize`, and the
first 45 lines of `def rebase_base`. In the test file, grep
`class TestBaseCommitPinning`, `class TestEpochMismatch`, and
`class TestMaterialize`; read only the relevant test methods. In the spec, read
the paragraphs anchored by `Known residual caveat: a **shallow clone**` and
`**BP-2.` through `**BP-6.` only.

**The defect.** `base_file_content` returns `None` for every nonzero `git show`,
conflating a genuinely absent path with an unavailable commit/object. In a real
`--depth 1` clone whose manifest named an older base, `refresh` reported:

```
a.c: exists in current upstream but not at the recorded base commit ...
upstream added this file ... --rebase-base
```

The old base commit was not present at all. The proposed remedy is wrong:
rebasing from a base the tool cannot read cannot preserve the package epoch.
The same ambiguity affects `--materialize`, and `rebase_base` can advance a
manifest without validating the old base when the working-file list is empty.

**The change.** Add one helper that validates a recorded commit with
`git cat-file -e <sha>^{commit}`. Its RuntimeError must name the SHA, say the
recorded base is unavailable (including shallow-clone/missing-history as the
usual cause), direct the developer to fetch/unshallow the history, and must not
recommend `--rebase-base`.

Call it at these load-bearing points:

- an existing `base_commit` in `ensure_base_commit`, before returning it;
- `rebase_base` for `old_base`, before the same-base no-op and before scanning
  working files, including an empty working area.

Newly initialized HEAD is already resolved by `resolve_head_commit`; do not add
a second policy there. After commit validation, make `base_file_content`
return `None` only when `src/c47/<rel>` is absent at that valid commit. Any
other extraction failure must be a RuntimeError with the command's stderr, not
silently reclassified as path absence.

Add regression tests for:

1. a real local `file://` shallow clone at depth 1 whose manifest records an
   older, unavailable base: `refresh` must raise the unavailable-base error,
   not the BP-4 upstream-added error;
2. `materialize` with an unavailable recorded base: same error and no working
   file created;
3. `rebase_base` with an unavailable old base and zero working files: error,
   manifest unchanged;
4. a valid base where the path truly did not exist: the existing BP-4
   upstream-added behavior remains unchanged.

Update the cited package-manager spec paragraphs to distinguish apply-side
shallow behavior from authoring-side behavior: refresh/materialize/rebase
require the recorded base commit and fail before classification when it is
unavailable.

**Tests that encode the old contract.** None. Keep the valid-base BP-4 tests;
they prove real path absence still has the existing contract.

**Gate:** `python3 tools/test_pkg_patch_refresh.py`, then
`./packages/forth-core/build-test.sh`.

*Verified mutation:* an actual depth-1 clone returned
`old_base_present_in_shallow_clone=False`, then current `refresh` emitted the
false “upstream added ... --rebase-base” diagnosis.

**Report:** paste the new shallow-clone test name/result, full RuntimeError
text, tooling-test summary, and both firmware success lines.

---

## R5-3 — Make rebase planning non-mutating until every merge succeeds

**File(s):** `tools/pkg_patch_refresh.py`,
`tools/test_pkg_patch_refresh.py`

**Tooling exemption:** this task is explicitly exempt from PREAMBLE rule 3.
Edits under `tools/` are the intended product edits.

**Read:** grep `def rebase_base` and read that function only. In the test file,
grep `class TestRebaseBase` and read that class only.

**The defect.** The existing “pre-scan” checks only old/new path existence.
The mutation pass runs `git merge-file` in place one file at a time. Every
positive return code is treated as an ordinary conflict, although Git returns
255 for a fatal merge-file error. Verified with sorted files `a.c` then binary
`b.dat`: `a.c` was merged, `b.dat` was labeled `conflicted` without any conflict
markers, and the manifest advanced to the new base. The working copy was left
between epochs, and the CLI's instruction to “resolve the markers” was
impossible. The split is not atomic.

**The change.** Turn rebase into plan-then-commit:

1. During planning, never pass a real working path as merge-file's output.
   Write current/old/new bytes to temporary inputs and invoke
   `git merge-file -p -L working -L base -L upstream <current> <old> <new>`.
   Capture its stdout bytes as the proposed file content.
2. Return code 0 is a clean merge. Return codes 1 through 127 are ordinary
   conflict counts; retain stdout with markers and classify the rel as
   `conflicted`. A negative subprocess return (signal) or any return >=128 is
   fatal: raise RuntimeError naming the rel, status, and stderr.
3. Fast-forward candidates (`working_bytes == old_bytes`) enter the same plan
   with `new_bytes`, but do not call merge-file.
4. Finish planning every rel before writing any working file or manifest.
5. Commit planned bytes with a same-directory temporary file plus `os.replace`,
   preserving the original file mode. Keep originals in memory. If any write or
   replace fails, restore every already-replaced file from its original bytes;
   leave `base_commit` unchanged and raise a loud RuntimeError. If restoration
   itself fails, report both the original error and every path that could not be
   restored.
6. Only after every planned file is installed successfully, set and save the
   new `base_commit`.

Do not change the specified behavior for genuine text conflicts: they leave
markers, are reported in `conflicted`, and the new base is recorded after the
successful whole-package commit.

Add a binary-second-file regression matching the verified scenario. Assert
RuntimeError, byte-identical `a.c` and `b.dat`, and old manifest base. Add a
write-failure regression by mocking the atomic-write helper on the second
install; assert rollback and old base. Existing clean-merge, fast-forward, and
marker-conflict tests remain unchanged.

**Tests that encode the old contract.** `test_rebase_conflict_leaves_markers_and_blocks_refresh`
encodes the valid positive-return conflict contract; leave it and make it pass.
`test_rebase_prescan_deleted_file_fatal_and_untouched` remains the path-level
pre-scan guard.

**Gate:** `python3 tools/test_pkg_patch_refresh.py`, then
`./packages/forth-core/build-test.sh`.

*Verified mutation:* a binary `b.dat` made merge-file return its fatal status;
current code returned `conflicted=['b.dat']`, mutated earlier `a.c`, and set
`manifest_base_advanced=True` instead of raising.

**Report:** paste both new test results, the fatal error text, tooling-test
summary, and both firmware success lines.

---

## R5-4 — Emit the promised drift warning before rebasing

**File(s):** `tools/pkg_patch_refresh.py`,
`tools/test_pkg_patch_refresh.py`

**Tooling exemption:** this task is explicitly exempt from PREAMBLE rule 3.
Edits under `tools/` are the intended product edits.

**Read:** grep and read only `def _check_drift`, the directory-list helpers it
calls, `def rebase_base`, and the `--rebase-base` output block in `main`. In the
test file, read only `class TestManifestDriftDetection` and
`class TestRebaseBase`.

**The defect.** The approved BP-6 text says drift between generated output and
the manifest “at rebase time is warn-and-proceed.” `rebase_base` never checks
either hash section and returns no `warnings` key. Verified by appending to a
generated patch and rebasing: the result keys were only old/new base and merge
classifications; there was no warning. A direct edit to generated truth is
therefore invisible during the operation that changes its epoch.

**The change.** Refactor drift message construction just enough to support a
read-only scan without lying that an overwrite already happened:

- keep refresh's existing per-entry warnings and self-heal behavior unchanged;
- add a helper that scans existing regular patch entries and recursive files/
  entries against their manifest hashes, returning warnings for missing records
  or hash mismatches;
- the rebase wording must say rebase leaves generated output unchanged and the
  next refresh will regenerate/self-heal it;
- run this scan before rebase planning, include `warnings` in every
  `rebase_base` result shape (including no-op/initialization), and print each
  warning to stderr from the CLI before normal rebase status lines;
- warning remains nonfatal, exactly as approved.

Add tests for a hand-edited patch and a hand-edited files/ entry. Each rebase
must return exactly one relevant warning while still completing its otherwise
valid base move. Add a clean rebase assertion with no warnings. Do not make
rebase regenerate generated output.

**Tests that encode the old contract.** None — the spec already requires this
warning. Existing refresh self-heal tests must remain unchanged.

**Gate:** `python3 tools/test_pkg_patch_refresh.py`, then
`./packages/forth-core/build-test.sh`.

*Verified mutation:* hand-editing `010-a.c.patch` before a valid rebase produced
`has_warnings=False`; `rebase_base` did not even return a warnings field.

**Report:** paste the three new test results, one exact rebase warning, tooling
summary, and both firmware success lines.

---

## R5-5 — Require the self-test success banner before declaring green

**File(s):** `packages/forth-core/build-test.sh`, `AGENTS.md`

**Read:** in `build-test.sh`, read only from the anchor
`# --- Run the gated self-test suite` to EOF. In `AGENTS.md`, read only the
`## Build & test` section.

**The defect.** The script defines success as process exit 0 only. It never
checks the required `FORTH SELF-TEST: ALL PASSED` banner. Verified by changing
only `if(forthDictSelfTest())` to `if(0 && forthDictSelfTest())`: the binary
printed no success banner, exited 0 through the following headless path, and
the script printed `==> BUILD + SELF-TEST GREEN.`. The c_args injection itself
does survive the normal reconfigure: Meson showed `c_args` empty before the
script's configure step and `-DFORTH_DEBUG_SELFTEST` after it. Removing the
define currently makes the all-target build fail to link, so AGENTS.md's claim
that a plain build necessarily finishes vacuously green is no longer accurate;
the banner hole is the demonstrated silent-green path.

**The change.** In the `DO_RUN=1` block:

1. Create a temporary log and install cleanup with `trap`.
2. Run the binary with combined stdout/stderr through `tee` so output remains
   visible. With `set +e`, capture the binary's status from `PIPESTATUS[0]`, not
   tee's status; restore `set -e` immediately afterward.
3. Keep the existing nonzero-status failure behavior.
4. After status 0, require an exact full line
   `FORTH SELF-TEST: ALL PASSED` with `grep -Fqx`. If absent, print a specific
   “suite did not run or did not report success” error and exit nonzero.
5. Print `==> BUILD + SELF-TEST GREEN.` only after both exit 0 and the banner
   check. Do not perform this check for `--build` because that mode explicitly
   skips execution.

Update AGENTS.md to state the proven facts: the gate refreshes, injects the
define after reconfigure, and independently requires the exact success banner;
exit 0 without the banner is red. Remove the inaccurate assurance that every
plain no-define build currently links and reports green. Keep the ban on
hand-rolled Meson/Ninja commands.

**Tests that encode the old contract.** None. This strengthens the gate's own
stated success contract.

**Facts the harness forces on you.** `errorf`/stdout ordering is irrelevant to
this exact-line existence check. Preserve the binary's real exit status through
the tee pipeline.

**Gate:** run `./packages/forth-core/build-test.sh` normally. Then perform the
same temporary marked mutation used by the audit:

```c
if(0 && forthDictSelfTest()) { /* AUDIT-PROBE R5: skip suite call */
```

Run the gate and confirm it exits nonzero specifically for the missing banner.
Manually restore that one line without git restore/checkout, run the gate again,
and confirm both success lines. End with no `AUDIT-PROBE R5` in the tree and no
diff in `config.c` or its generated patch/manifest beyond legitimate earlier
tasks.

*Verified mutation:* with the suite-call bypass above, the current gate exited
0 and printed `BUILD + SELF-TEST GREEN.` without any `FORTH SELF-TEST` banner.

**Report:** paste the missing-banner failure line, final two success lines, and
`git diff --check` result.

---

## R5-7 — Make a corrupt manifest fatal, and make saving it atomic

**Approved by the architect 2026-07-15 (R5-A1). Both halves are one contract —
implement them together or not at all.**

**File(s):** `tools/pkg_patch_refresh.py`, `tools/test_pkg_patch_refresh.py`

**Read:** in `pkg_patch_refresh.py`, read `load_manifest`, `save_manifest`,
`resolve_head_commit` and `ensure_base_commit` (they are consecutive, roughly
lines 143-198) and nothing else. In `test_pkg_patch_refresh.py`, read the
`_TempProject` class and `test_legacy_manifest_init_warns`; copy their idioms.

**The defect.** `load_manifest` catches `(ValueError, OSError)` and returns
`{'patches': {}, 'files': {}}` — a value indistinguishable from a fresh
package, and carrying no `base_commit` key. `ensure_base_commit` then reads
`manifest.get('base_commit')` as `None`, finds `manifest['patches']` and
`manifest['files']` both empty so its legacy warning does not fire, and records
current HEAD as the base. A package pinned to base A, with upstream since moved
to B, therefore silently re-pins to B and regenerates patches that revert
upstream's own change. R5 reproduced exactly this: `repinned_to_new_head=True`,
one generic "patch exists but was not recorded" warning, and a patch containing
`-line4_UPSTREAM` / `+line4`.

`save_manifest` makes that state reachable without a human editing anything:
`open(path, 'w')` truncates before writing, so an interrupted or crashed process
leaves a truncated file, which `load_manifest` then reads as "fresh package".
The two defects feed each other.

**The change — `load_manifest`.** Distinguish absent from unreadable:

```
path = <pkgdir_abs>/.refresh-manifest.json
if not os.path.isfile(path):
    return {'patches': {}, 'files': {}}      # genuinely new package: OK
try:
    with open(path) as f:
        data = json.load(f)
except (ValueError, OSError) as exc:
    raise RuntimeError(                       # NEVER treat as fresh
        f'{path} exists but is unreadable or is not valid JSON ({exc}). '
        f'Refusing to treat a corrupt manifest as a fresh package: that '
        f'would re-pin base_commit to current HEAD and emit patches that '
        f'revert upstream changes (R5-A1). Restore it from git '
        f'(git checkout -- {path}) or delete it deliberately to '
        f're-initialize this package.')
if not isinstance(data, dict):
    raise RuntimeError(<same message, ' — top-level JSON is not an object'>)
data.setdefault('patches', {})
data.setdefault('files', {})
return data
```

Rules, all binding:
- The file **existing but unreadable** is fatal. Only a genuinely absent file
  initializes a package. Do not use a bare `except:`; catch exactly
  `(ValueError, OSError)` as today.
- `json.load` succeeding but yielding a non-dict (a list, a string, `null`) is
  the same class of corruption and is equally fatal — today it would reach
  `data.setdefault` and die with an unhelpful `AttributeError`.
- `RuntimeError` is the tool's existing fatal idiom (see `resolve_head_commit`,
  :170). Use it. Do not call `sys.exit`, and do not add a new exception class.
- Do not touch `ensure_base_commit`. With load fixed it can no longer be
  reached with a silently-emptied manifest, and its legacy-init warning is
  still correct for real legacy packages.

**The change — `save_manifest`.** Write through a same-directory temporary file
so the previous valid manifest survives interruption:

```
path = <pkgdir_abs>/.refresh-manifest.json
fd, tmp = tempfile.mkstemp(dir=<pkgdir_abs>, prefix='.refresh-manifest.', suffix='.tmp')
try:
    with os.fdopen(fd, 'w') as f:
        json.dump(manifest, f, indent=2, sort_keys=True)
        f.write('\n')
        f.flush()
        os.fsync(f.fileno())      # bytes on disk BEFORE the rename
    os.replace(tmp, path)         # atomic on POSIX and Windows
except BaseException:
    try: os.unlink(tmp)           # never leave .tmp litter behind
    except OSError: pass
    raise
```

Rules:
- The temp file **must** be in the same directory as the target — `os.replace`
  is only atomic within a filesystem, and `/tmp` may be a different one.
- `fsync` before `os.replace`, not after. The point is that the rename can never
  publish a file whose bytes have not landed.
- Catch `BaseException`, not `Exception`: a `KeyboardInterrupt` between mkstemp
  and replace is exactly the interruption this exists to survive, and it must
  still clean up and re-raise.
- Keep the output byte-identical to today (`indent=2`, `sort_keys=True`,
  trailing newline). This must not produce a manifest diff on an unchanged
  package — verify with `git diff --stat` after a refresh.
- `tempfile` is already imported by the test module, not necessarily by
  `pkg_patch_refresh.py`. Add the import if missing.

**Tests** — add to `tools/test_pkg_patch_refresh.py`, using `_TempProject`:

1. `test_corrupt_manifest_is_fatal` — write upstream `foo.c` at base A, a
   working override, and a valid manifest via `save_manifest`; capture
   `base_commit`. Truncate the manifest to invalid JSON (e.g. write `'{"patc'`).
   Assert `p.refresh()` raises `RuntimeError`, and that the message names the
   manifest path. **BUG THIS TEST EXISTS TO CATCH:** a corrupt manifest read as
   a fresh package.
2. `test_corrupt_manifest_leaves_patch_working_and_base_untouched` — same setup;
   record the bytes of the generated patch, the working file, and the manifest's
   `base_commit`. Corrupt the manifest, `assertRaises(RuntimeError)`, then assert
   all three are byte-for-byte unchanged. This is R5-A1's actual requirement:
   the failure must be inert, not partial.
3. `test_non_dict_manifest_is_fatal` — manifest containing `[]`. Assert
   `RuntimeError`, not `AttributeError`.
4. `test_missing_manifest_still_initializes` — no manifest at all. Assert
   refresh succeeds and records a `base_commit`. **This must keep passing:** the
   change must not make a genuinely new package fatal.
5. `test_save_manifest_leaves_no_temp_files` — call `save_manifest`, then assert
   no entry in the package directory matches `.refresh-manifest.*.tmp`.
6. `test_save_manifest_survives_interrupted_write` — write a valid manifest,
   then monkeypatch `json.dump` to raise `KeyboardInterrupt`, call
   `save_manifest` inside `assertRaises(KeyboardInterrupt)`, and assert the
   ORIGINAL manifest is still on disk byte-for-byte and no `.tmp` litter remains.
   **BUG THIS TEST EXISTS TO CATCH:** the in-place truncating write that made
   the corrupt state reachable in the first place.

**Gate.** `python3 -m unittest discover -s tools -p 'test_pkg_*.py'` green, then
`./packages/forth-core/build-test.sh` green — the refresh must still produce a
byte-identical manifest for forth-core (`git diff --stat` shows no manifest
change). Report both.

---

## R5-6 — Run the final tooling/build gates and commit once

**File(s):** all paths changed by R5-1 through R5-5 **and R5-7**, plus refresh-generated
`packages/forth-core/patches/*.patch` and
`packages/forth-core/.refresh-manifest.json`

**Tooling exemption:** the earlier named `tools/` changes are intended. Do not
introduce any new source edit in this task.

**Read:** do not read source again. Start from `git status --short` and inspect
the complete diff for only the paths named by the earlier tasks. Confirm no
`AUDIT-PROBE R5` remains.

**The defect.** None new. This task prevents the fix series from ending with a
unit-only green, stale generated outputs, or an uncommitted probe.

**Ordering:** this task runs LAST. R5-7 is numbered after R5-5 but is sequenced
before this one — it must be complete and green before you start here.

**The change.** Make no product change. Run, in order:

1. `python3 tools/test_pkg_patch_refresh.py`
2. `./packages/forth-core/build-test.sh`
3. `git diff --check`
4. inspect `git status --short` and the full diff

Run `./packages/forth-core/build-test.sh` a second time and confirm it produces
no additional patch/manifest diff; this is the cross-run determinism check for
R5-1. Confirm both build runs contain the self-test success banner and green
line.

Stage only explicit changed paths (never `git add -A`) and commit once with:

```
pkg: harden refresh base and self-test gate
```

Do not push.

**Tests that encode the old contract.** None.

**Gate:** both commands above green, second build produces no new diff,
`git diff --check` clean.

*Verified mutation:* UNVERIFIED — this is the integration/commit task; its
proof is the two consecutive real gates after Tasks R5-1 through R5-5.

**Report:** paste tooling test count/result, both firmware success lines from
the final run, confirmation that the second run caused no new generated diff,
commit SHA, and final `git status --short`.

---

## Reviewed leads with no implementation task

- The transient commit/manifest mismatch self-heals once as claimed: the first
  refresh emitted one warning and rewrote the hash; the next refresh emitted
  zero warnings. It does not warn forever.
- The self-test c_args injection is applied after normal reconfigure and was
  observed in Meson's regenerated configuration. The independent banner check
  in R5-5 closes the remaining demonstrated silent-green path.
- The `.pkgignore` and clean-clone policy questions require design decisions;
  they are recorded in `FOR_THE_ARCHITECT_R5.md`, not improvised here.
