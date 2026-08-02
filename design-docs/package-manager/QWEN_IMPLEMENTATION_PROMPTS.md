# Qwen Implementation Prompt Sequence — Patch-Based Package Overlay System

> **ARCHIVED (2026-07-15) — NEVER EXECUTE ANY PROMPT IN THIS FILE.**
> These prompts implement **revision 1** of the package system — the
> libclang/function-boundary design (see Prompt 2) that was implemented and
> then **reverted** on this branch (preserved at
> `checkpoint/pre-plain-diff-revert-20260712-1541`).
> `design-docs/package-manager/PROPOSED_SPEC_CHANGES.md` is now **revision 2**
> (plain-diff), which supersedes the spec these prompts were written
> against; the implemented revision-2 system is documented in
> `design-docs/package-manager/README.md` and `design-docs/package-manager/IMPLEMENTATION_REPORT.md`.
> Running any prompt below would begin rebuilding the reverted design on top
> of the live one. Historical record only.

Source spec: `design-docs/package-manager/PROPOSED_SPEC_CHANGES.md` (approved). Branch:
`package-manager/patch-based-overlay`. Run prompts **in order** — each
assumes the previous one landed and the tree is green.

Every prompt below is self-contained: paste it to Qwen on its own. Do not
skip ahead based on assumed context from an earlier prompt's conversation —
each restates the paths and constraints it needs.

Each prompt in the sequence below **begins with this preamble, verbatim**.
Where a prompt needs a rule sharpened for its own step (e.g. Prompt 4's
empirical check, Prompt 6's synthetic-package cleanup), that prompt adds to
the preamble — it never contradicts it.

---

## PREAMBLE (prepend verbatim to every prompt below)

You are implementing ONE step of a planned series that builds the
**patch-based package overlay system** — the C47 custom package manager's
tooling — in the repo at `/home/stan/c43`, on branch
`package-manager/patch-based-overlay` (already checked out — stay on it).
This is package-manager tooling work: Python tools under `tools/`, the
top-level `meson.build` overlay wiring, and `design-docs/package-manager/` documentation.
It is not C firmware work and not Forth work. Rules, non-negotiable:

1. **SCOPE.** Implement exactly what this prompt specifies. Nothing else. If
   the spec and the code disagree, STOP and report the conflict — do not
   improvise. This series touches the package overlay/manager machinery
   ONLY: `tools/resolve_c47_src.py` (or its replacement), the new patch
   generation/application tools this series adds, `custom_package/`, and the
   `if custom_pkg_list != []` overlay blocks in the top-level `meson.build`.
   It does NOT modify, or propose fixes for, any actual package payload or
   firmware source — not anything under `packages/forth-core/`, not
   anything under `src/c47/`, not any other package's contents. Those are
   *inputs* the tooling operates on, never things this series edits. If your
   investigation reveals a bug in a package payload or in upstream, note it
   as a `[GAP]` in your report and do not act on it.

2. **UPSTREAM AND PACKAGE PAYLOADS ARE READ-ONLY.** Never edit anything under
   `src/c47/` (upstream, vendored) — the whole point of the overlay system
   is that packages never touch it. Also never edit an existing package's
   payload files (e.g. the override sources under `packages/forth-core/`);
   this series builds the *machinery*, not the packages that ride on it. The
   only source trees you create or modify are the tooling paths named in the
   prompt, plus any clearly-named **scratch** package a prompt tells you to
   construct for a test (and then delete). Reading `src/c47/<rel>` — to diff
   against, or to `git hash-object` for a real blob SHA — is required and
   fine; writing there, even as a temporary scratch step, is never
   permitted.

3. **AUTHORITY.** `design-docs/package-manager/PROPOSED_SPEC_CHANGES.md` is the single
   authority for this series — no `design-docs/package-manager/DESIGN.md` exists yet;
   that spec document is what would eventually seed one. Read the § slice
   named in the prompt BEFORE coding. Do not relitigate anything marked
   `[RATIFIED]`. The spec is written only by the design/review side; as an
   implementer, the only edit you make to it is flipping a
   `[VERIFIED: pending]` marker to a real citation once you have empirically
   confirmed the claim (Prompt 4 does this for §5). Never change a
   `[RATIFIED]` decision, and never resolve an open question that a *later*
   prompt in this sequence owns, just because you judge an answer obvious.

4. **LOCKED DECISIONS (apply everywhere in this series):**
   - Explicit override-target declaration, not path-mirroring alone (§2,
     ratified): every patch's on-disk filename and its internal `+++ b/...`
     diff header must independently agree on the upstream target;
     disagreement, or a target that resolves nowhere, is a fatal error,
     never a silent "must be a new file" fallback.
   - libclang stays out of the build path (§4, ratified): `clang.cindex` is
     a dependency of authoring tooling only (`tools/pkg_patch_extract.py`
     and whatever imports it). It must never be reachable from
     `tools/resolve_c47_src.py` or anything a bare `meson`/`ninja` invokes.
     If a change you're making would require importing it there, STOP and
     report `[DECISION NEEDED]` — do not add the import for convenience.
   - Conflict-marker scan is mandatory, not implied by exit code (§5,
     ratified): every `git apply -3` call, anywhere in this series, is
     followed by an unconditional scan of the result for `<<<<<<<`
     / `=======` / `>>>>>>>` line-starts, before that content is treated as
     valid. A clean `git apply -3` exit code alone is never sufficient.
   - Same-function conflicts fail loudly, always (§7, ratified): if a patch
     stack's cumulative application would silently prefer one package's
     content over another's colliding edit, that is the specific bug this
     series exists to prevent — it must never happen without an explicit,
     named build failure.
   - Whole-file override and function-level patch are mutually exclusive
     per target file (§8, ratified): enforced by the tool at configure
     time, before any shadow-tree mutation — never left to authoring
     discipline or documentation.
   - Ordering for a cumulative patch stack (§3) is `(ordinal, CUSTOM_PKG
     list position)` — reusing the existing, already-explicit `CUSTOM_PKG`
     list order as the tie-break, not a new series/manifest file.
   - If an implementation choice in front of you seems to require inventing
     a mechanism not named in `PROPOSED_SPEC_CHANGES.md` or in this prompt,
     STOP and report `[DECISION NEEDED]`. Do not silently pick an unstated
     behavior: if the spec doesn't name it, you don't add it.

5. **TESTS.** Every added test is a real, runnable test in this repo's
   existing test infrastructure for the tool it covers — a `meson test`
   entry for Python tooling under `tools/`, following whatever test-runner
   pattern `meson.build` already uses (check before inventing a new one).
   Each test's docstring/comment names the escaping mutation: the specific
   bug that must make it fail. **MUTATION CHECK, every test, no
   exceptions:** after the test passes, temporarily re-introduce that exact
   bug in a scratch copy (never commit the broken version), rerun, confirm
   the test FAILS, revert, confirm it passes again. Report the mutation
   run's actual output — the real pass/fail transcript, not a description
   of what you expect would happen — in your final report for that prompt.

6. **GATE (build/verify before reporting, every prompt):**
   - Run `meson test -C build.sim`. If it was passing before your change
     and fails after, that's a regression — fix it before reporting done.
     If `build.sim` isn't configured yet, configure it per `BUILD.md` with
     no `CUSTOM_PKG` set (vanilla) first, confirm green, then proceed.
   - Where a prompt names an additional gate (e.g. Prompt 6's real Meson
     `custom_target()` build with two scratch packages active, Prompt 8's
     full vanilla-build regression pass), run that too — it is not optional
     on top of the base gate above.

7. **STOP.** When done: do NOT `git commit`, do NOT push. Leave the working
   tree modified. Write a report: what changed (file:line), test results,
   mutation-check transcript(s), gate output tail, and any
   `[DECISION NEEDED]` / `[GAP]` found. A human — or a separate, later step
   — performs the commit on this branch.

8. **TWO-ATTEMPT DEBUGGER HANDOFF.** This rule applies only when this prompt
   authorizes you to fix the observed error; it never overrides an earlier
   immediate-STOP rule. After a command, test, or gate first fails because of
   your task changes, you may make at most two distinct repair attempts. A
   repair attempt is an edit intended to clear that failure followed by
   rerunning the relevant command. The original task implementation is not a
   repair attempt. If the required command is still not green after repair
   attempt 2 — even if the visible error changes — STOP. Do not make a third
   repair, broaden scope, or use git to undo anything. Leave the tree exactly
   as it stands; read-only inspection is allowed only to prepare this report:

   `[SOL DEBUGGER HANDOFF]`

   - task ID and exact failing command;
   - original failure and its relevant verbatim output;
   - attempt 1: files/hunks changed, rationale, and resulting output;
   - attempt 2: files/hunks changed, rationale, and resulting output;
   - current `git status --short`, `git diff --stat`, and relevant diff excerpts;
   - your best remaining hypotheses and anything that surprised you.

---

## Prompt 1 — Patch Storage Convention, Cross-Check, and README

**Implements:** `PROPOSED_SPEC_CHANGES.md` §1 (Storage Format) and the
ratified §2 decision (explicit override-target declaration, not
path-mirroring alone).

**Files to create/modify:**
- `tools/pkg_patch_common.py` (new) — shared parsing/validation helpers used
  by both the build-time resolver and the future authoring tool.
- `tools/resolve_c47_src.py` — import and use the new validation helper; do
  not otherwise change its shadow-tree logic yet (that's Prompt 7).
- `design-docs/package-manager/README.md` — document the new convention (add a section,
  do not remove existing whole-file-override documentation — that mechanism
  still exists per §8).

**Convention to implement:**
1. A function-level patch for package `pkgdir` targeting upstream file
   `src/c47/<rel>` lives at:
   `packages/<pkgdir>/patches/<NNN>-<rel_encoded>.patch`
   where `<NNN>` is a zero-padded 3-digit ordinal (`010`, `020`, …) and
   `<rel_encoded>` is `<rel>` with every `/` replaced by `__` (e.g.
   `programming/manage.c` → `programming__manage.c`).
2. Every patch file's content must be a unified diff (`git diff`-format,
   `-U3` or greater context) whose `+++ b/...` header line names the
   upstream target. This is the **second, independent signal** required by
   the ratified §2 decision.
3. Write `tools/pkg_patch_common.py` with these exact functions:

```python
def decode_patch_filename(filename):
    """'<NNN>-<rel_encoded>.patch' -> (ordinal: int, rel: str).
    Raises ValueError with a clear message on any malformed filename
    (missing NNN-, missing .patch, empty rel)."""

def parse_patch_target(patch_file_path):
    """Read the patch file, find the first '+++ b/...' header line,
    strip the 'b/' prefix and any leading 'src/c47/', return the bare
    rel path (e.g. 'keyboard.c', 'programming/manage.c').
    Raises ValueError if no '+++ b/...' line is found."""

def validate_patch_declaration(pkgdir, patch_filename, project_root):
    """Cross-check the two independent signals:
      - rel_from_name = decode_patch_filename(patch_filename)[1]
      - rel_from_header = parse_patch_target(full path to the patch file)
    Fatal (raise ValueError with both values in the message) if they
    disagree. Fatal if neither resolves to a real file under
    project_root/src/c47/<rel>. Returns rel on success."""
```

4. In `tools/resolve_c47_src.py`, wherever a new `pkg_patch_sources` list
   would eventually be consumed (Prompt 7 wires this up for real — for this
   prompt, just make `validate_patch_declaration` importable and add a unit
   test proving it works; do not wire it into the shadow-tree build yet).

**Mutation-test requirement:** add `tools/test_pkg_patch_common.py` (or
extend an existing test runner if one exists — check `BUILD.md` and
`meson.build` for how Python tooling tests are currently run, e.g. via
`meson test`). At minimum:
- `test_decode_patch_filename_roundtrip` — encode/decode agreement for a
  nested path (`ui/tam.c`).
- `test_decode_patch_filename_rejects_malformed` — missing `NNN-` prefix,
  missing `.patch` suffix, empty rel each raise `ValueError`.
- `test_validate_patch_declaration_agrees` — filename and header name the
  same rel, matching upstream file exists → returns rel, no exception.
- `test_validate_patch_declaration_catches_mismatch` — **this is the bug
  this whole prompt exists to prevent.** Construct a patch file whose
  filename says `010-keyboard.c.patch` but whose `+++ b/...` header says
  `screen.c`. Confirm `validate_patch_declaration` raises. Then, as a
  spot-check, comment out the header-vs-filename comparison in your own
  working copy, confirm this test now fails (silently accepts the
  mismatch), then restore the comparison and confirm it passes again. This
  is the concrete bug the test must catch: a typo'd patch filename being
  silently treated as fine because only one of the two signals was checked.
- `test_validate_patch_declaration_catches_no_upstream_match` — both
  signals agree on a rel that doesn't exist under `src/c47/` → raises,
  does NOT silently fall through to "treat as new file."

Add the README section under a new `## Function-Level Patch Overrides
(new)` heading, cross-referencing the existing `## Override Semantics`
section rather than duplicating it. State plainly: whole-file overrides
(`pkg_override_sources`) and function-level patches (`pkg_patch_sources`,
wired up in a later prompt) are two distinct mechanisms; §8's
mutual-exclusivity rule (enforced in Prompt 7) means a given upstream file
may be targeted by exactly one of the two, never both.

---

## Prompt 2 — Function-Boundary Extractor (libclang)

**Implements:** `PROPOSED_SPEC_CHANGES.md` §4 (Patch Granularity), including
the ratified constraint that libclang must never be reachable from the
build-time path.

**Files to create:**
- `tools/pkg_patch_extract.py` (new) — authoring-time only. Must not be
  imported by `tools/resolve_c47_src.py` or anything reachable from a
  `meson`/`ninja` invocation.

**Precondition this tool must assert, not assume:** it needs clang compile
flags for `src/c47/<rel>` from a **vanilla** (no `CUSTOM_PKG`) build's
`compile_commands.json` — `build.sim/compile_commands.json` configured with
`CUSTOM_PKG` unset — so lookups resolve directly to `src/c47/<rel>` paths,
not paths inside a package's `custom_pkg_shadow/`. If the vanilla
`build.sim/compile_commands.json` doesn't exist, error out and tell the
user to configure it first per `BUILD.md`; do not try to auto-configure it
yourself.

**Exact function signatures:**

```python
def list_function_ranges(file_path, compile_commands_json_path):
    """Return a list of (name: str, start_line: int, end_line: int),
    1-indexed inclusive, for every function *definition* (not
    declaration/prototype) in file_path, using clang.cindex against the
    real compile args for file_path found in compile_commands_json_path.

    - Uses clang.cindex.Index.create() and Index.parse() with the args
      list resolved from the compile_commands.json entry whose 'file'
      field matches os.path.realpath(file_path). Fatal error (raise, do
      not fall back to guessing flags) if no matching entry exists.
    - Walks translation_unit.cursor.walk_preorder(), filtering
      cursor.kind == clang.cindex.CursorKind.FUNCTION_DECL and
      cursor.is_definition() and cursor.location.file is not None and
      os.path.realpath(cursor.location.file.name) == os.path.realpath(file_path)
      (the file-identity check matters: libclang will walk into included
      headers otherwise, e.g. defines.h, and you do not want their
      functions in this file's range list).
    - start_line/end_line come from cursor.extent.start.line /
      cursor.extent.end.line — this is libclang's actual parsed AST
      extent, which is correct for braces inside string/char literals and
      for macro-expanded bodies (the extent reflects the expansion, not
      raw token brace-counting), and for #ifdef-guarded bodies clang
      parses only the active branch as determined by the same
      preprocessor defines used in the real build (from
      compile_commands.json), so an inactive #ifdef branch's braces never
      appear in the AST at all — do not add separate #ifdef handling on
      top of this; the compile args already resolve it correctly.
    """

def function_at_line(ranges, line):
    """Given the list from list_function_ranges and a 1-indexed line
    number, return the (name, start, end) tuple whose range contains
    line, or None if no function's range contains it (global/macro/struct/
    file-scope code)."""
```

**Test-case requirement (do not skip, do not assume it would work):**
search the vendored `src/c47/` tree in this repo for at least one real
function containing one of these patterns, and write a test proving
`list_function_ranges` handles it correctly:
- a function with a brace inside a string or char literal, OR
- a function with an `#ifdef`/`#endif`-guarded body, OR
- a function whose body is (or contains) a macro-expanded block with
  braces.

If, after an actual search (not a guess), no such pattern exists anywhere
in `src/c47/`, report `[UNABLE TO VERIFY: no matching pattern found in
tree]` for that specific sub-case and use a synthetic single-file test
fixture instead (a small `.c` file under
`tools/test_fixtures/pkg_patch_extract/`, with its own tiny
`compile_commands.json` entry) that constructs the pattern deliberately.
Either way, the test must assert the extracted `end_line` is where the
function's real closing brace is (per libclang), not where a naive
brace-counter would stop (e.g. at a `}` inside a string literal, which
would be earlier than the real end).

**Mutation-test requirement:** add a test that deliberately breaks the
file-identity check (remove the
`os.path.realpath(cursor.location.file.name) == os.path.realpath(file_path)`
filter in a scratch copy), confirm the extractor then wrongly includes a
function from an included header in the results for a file that includes
`defines.h`, confirm the test catches this, then restore the filter.

---

## Prompt 3 — Patch Generation (`refresh` command)

**Implements:** `PROPOSED_SPEC_CHANGES.md` §1, §4, §6 (materialize/refresh
workflow), and prepares the ground for §5's empirical blob-ancestry check
(actually performed in Prompt 4).

**Files to create:**
- `tools/pkg_patch_refresh.py` (new) — authoring-time CLI:
  `python3 tools/pkg_patch_refresh.py <pkgdir> <rel>`.

**Workflow this implements (materialize/refresh, §6 resolution):** the
materialized working file continues to live at `packages/<pkgdir>/<rel>`
(today's whole-file-override convention, unchanged — this is what a
developer edits with full LSP context). `refresh` diffs that materialized
file against the real, currently-committed `src/c47/<rel>` in this repo,
and writes one `.patch` file per changed function under
`packages/<pkgdir>/patches/`. Both the materialized file and the generated
patch(es) are committed to git — the patch is the build input (Prompt 7
consumes it), the materialized file is kept for review/LSP ergonomics and
as `refresh`'s input on the next edit.

**Exact procedure — implement precisely, this is the part item 5 flags as
empirically uncertain, so get the mechanics exactly right so Prompt 4 can
test them:**

```python
def refresh(pkgdir, rel, project_root):
    src_path = f'{project_root}/src/c47/{rel}'
    materialized_path = f'{project_root}/packages/{pkgdir}/{rel}'
    assert file_exists(src_path), f'no upstream file at src/c47/{rel}'
    assert file_exists(materialized_path), \
        f'no materialized working file at packages/{pkgdir}/{rel} — ' \
        f'copy src/c47/{rel} there first and edit it'

    # Real, resolvable pre-image blob SHA — src_path is a tracked,
    # committed file in THIS repo's own git history, so this SHA is a
    # real object git can resolve (unlike a SHA from a separate clone).
    blob_sha = run(['git', 'hash-object', src_path]).stdout.strip()
    assert run(['git', 'cat-file', '-e', blob_sha]).returncode == 0, \
        'pre-image blob unexpectedly not a resolvable git object — STOP, ' \
        'this breaks the -3 ancestry assumption, report as [DECISION NEEDED]'

    raw_diff = run(['git', 'diff', '--no-index', '-U3',
                     '--src-prefix=a/', '--dst-prefix=b/',
                     src_path, materialized_path]).stdout
    if raw_diff == '':
        print(f'no changes between src/c47/{rel} and packages/{pkgdir}/{rel} '
              f'— nothing to refresh')
        return

    # Rewrite both header paths to the SAME logical target
    # (src/c47/<rel>) and inject a real index line using blob_sha as the
    # pre-image. Post-image SHA is unresolvable pre-apply, use
    # '0000000000000000000000000000000000000000' as a literal placeholder
    # — git apply only needs the PRE-image SHA to find the merge base for
    # -3; a placeholder post-image SHA is standard practice (this is what
    # 'git diff' itself does for an uncommitted working-tree change) and
    # does not block apply.
    patched = rewrite_diff_headers(
        raw_diff,
        a_header=f'a/src/c47/{rel}', b_header=f'b/src/c47/{rel}',
        index_line=f'index {blob_sha}..0000000000000000000000000000000000000000 100644')

    ranges = list_function_ranges(src_path, vanilla_compile_commands_json_path)
    hunks = split_diff_into_hunks(patched)  # one entry per @@ ... @@ block
    by_function = {}
    orphan_hunks = []
    for hunk in hunks:
        start_line_in_preimage = hunk.at_a_start  # from the @@ -a,b +c,d @@ line
        fn = function_at_line(ranges, start_line_in_preimage)
        if fn is None:
            orphan_hunks.append(hunk)
        else:
            by_function.setdefault(fn.name, []).append(hunk)

    if orphan_hunks:
        # §8: a hunk outside every function boundary means this change is
        # NOT function-scoped (global/macro/struct/typedef). Refuse to
        # generate a function-patch for it — direct the author to
        # pkg_override_sources (whole-file) instead, per §8's ratified
        # mutual-exclusivity rule.
        fail(f'{len(orphan_hunks)} hunk(s) in {rel} fall outside any '
             f'function boundary (lines: {[h.at_a_start for h in orphan_hunks]}). '
             f'This file needs a whole-file override (pkg_override_sources), '
             f'not a function-level patch — the two mechanisms are mutually '
             f'exclusive per file (§8).')

    existing = list_existing_patches(pkgdir, rel)  # parse dir for this rel
    next_ordinal = (max(o for o, _ in existing) + 10) if existing else 10
    for fn_name, fn_hunks in by_function.items():
        ordinal = ordinal_for_function(fn_name, existing, default=next_ordinal)
        out_name = f'{ordinal:03d}-{rel.replace("/", "__")}.patch'
        write_patch_file(pkgdir, out_name, reassemble(patched_header, fn_hunks))
```

(`ordinal_for_function`, `list_existing_patches`, `split_diff_into_hunks`,
`reassemble` are helper functions you must write; keep them in
`tools/pkg_patch_refresh.py`. If a package already has a patch for this
exact function, `refresh` must overwrite that same file in place — reusing
its existing ordinal — rather than allocating a new one, so re-running
`refresh` after further edits doesn't accumulate duplicate patches for the
same function.)

**Mutation-test requirement:**
- `test_refresh_rejects_non_function_hunk` — materialize a change to a
  global variable's initializer (not inside any function), confirm
  `refresh` fails with the §8 message rather than silently producing a
  patch. Spot-check: remove the `orphan_hunks` check, confirm a patch is
  wrongly generated, restore the check.
- `test_refresh_reuses_ordinal_on_second_call` — refresh a function once,
  edit it again, refresh again, confirm exactly one patch file exists for
  that function (not two).
- `test_refresh_blob_sha_resolvable` — after generating a patch, run
  `git cat-file -e <the injected pre-image sha>` and confirm it succeeds.
  This is a real (not simulated) piece of the §5 empirical question —
  record the result plainly in your final report for this prompt.

---

## Prompt 4 — Patch Application, Materialization, and the §5 Empirical Check

**Implements:** `PROPOSED_SPEC_CHANGES.md` §5 (Application Mechanism),
including the `[VERIFIED: pending]` blob-ancestry question — **this prompt
is where that gets answered empirically, not assumed.**

**Files to create/modify:**
- `tools/pkg_patch_apply.py` (new) — build-time-safe (no libclang import).
  Function: `apply_patch(patch_path, src_c47_dir, dest_path) -> None`.

**Exact procedure:**

```python
def apply_patch(patch_path, src_c47_dir, dest_path):
    rel = validate_patch_declaration(...)   # from Prompt 1's module
    src_file = f'{src_c47_dir}/{rel}'
    shutil.copy2(src_file, dest_path)       # materialize current upstream copy
    result = run(['git', 'apply', '-3', '--directory', ...,  # see note below
                  patch_path], cwd=<repo root>)
    if result.returncode != 0:
        fail(f'git apply -3 failed outright for {patch_path} against '
             f'{rel}: {result.stderr}')

    # RATIFIED §5: -3 exiting 0 is NOT sufficient. Scan unconditionally.
    content = read(dest_path)
    if re.search(r'^(<{7}|={7}|>{7})', content, re.MULTILINE):
        fail(f'{patch_path} applied against {rel} but left conflict '
             f'markers in the result (git apply -3 "succeeded" by '
             f'three-way-merging into a conflicted state, not a clean '
             f'merge). This must fail the build per §5/§7 — do not let a '
             f'file containing conflict markers reach the compiler.')
```

(Work out the exact `git apply` invocation needed to apply a patch whose
target is `src/c47/<rel>` onto a *copy* sitting at `dest_path` — you will
likely need `--directory` or to apply against a temp checkout and copy the
result, since `git apply` normally operates against the working tree via
paths recorded in the patch, not an arbitrary destination path. Get this
mechanically correct; this is exactly the kind of detail the empirical
check below needs to be honest about.)

**The empirical check — do this for real, report the real outcome:**
1. Using Prompt 3's `refresh`, generate at least one real patch against a
   real `src/c47/` file in this repo (pick something small).
2. Apply it via `apply_patch` above against the *current* `src/c47/<rel>`.
   Confirm clean apply.
3. Now simulate drift: using the separate untracked `upstream/` clone
   already present in this repo's root (a full clone of
   `https://gitlab.com/rpncalculators/c43.git`) — **read-only, do not
   modify it** — find a commit that changed the same file, check out that
   older or newer revision of the file *content only* (e.g.
   `git -C upstream show <rev>:src/c47/<rel>` piped to a scratch copy, not
   an actual checkout of this repo's `src/c47/`), and re-run `apply_patch`
   against that drifted content in a scratch destination. Record: clean
   apply / three-way merge with conflict markers (caught by the scan
   above) / outright failure.
4. Report the real answer to: does the injected `index` line's blob SHA
   actually enable a three-way merge here, or does `git apply -3`
   effectively behave like a plain apply because the "base" it merges
   against is whatever `src_file` currently contains rather than a
   genuinely different historical blob? (This distinction matters: Prompt
   3's `blob_sha` is always `git hash-object` of the *current* `src_path`
   at generation time, so if `src_path` hasn't changed since generation,
   there is no real ancestry divergence to test — you may need to
   construct a scratch scenario where the patch was generated against one
   version and applied against a genuinely different one, using the
   `upstream/` clone's history for that older/newer version, to test this
   honestly.)

Write the real outcome — not an assumption — into a new file
`design-docs/package-manager/PATCH_ANCESTRY_FINDINGS.md`, and reference it from
`PROPOSED_SPEC_CHANGES.md` §5 by changing `[VERIFIED: pending]` to
`[VERIFIED: design-docs/package-manager/PATCH_ANCESTRY_FINDINGS.md]`.

**Mutation-test requirement:**
- `test_apply_patch_catches_conflict_markers` — construct a patch and a
  drifted target such that `git apply -3` succeeds but leaves markers
  (from step 3 above, or synthetically if real drift doesn't produce
  one). Confirm `apply_patch` fails. Spot-check: comment out the marker
  scan, confirm the same input now "succeeds" and produces a file with
  literal `<<<<<<<` in it, restore the scan.
- `test_apply_patch_catches_outright_failure` — a patch that cannot apply
  at all (e.g. target function no longer exists) → `apply_patch` fails
  with a clear message, does not silently skip the file.

---

## Prompt 5 — Ordering Enforcement for Cumulative Per-File Patch Stacks

**Implements:** `PROPOSED_SPEC_CHANGES.md` §3 (Composition).

**Files to modify:** `tools/resolve_c47_src.py` (shadow-mode path).

**Exact ordering rule (resolves the "open question" left in §3):**
1. For a given upstream rel path, collect every `.patch` file across every
   package named in the active `CUSTOM_PKG` list (in the order given in
   that comma-separated list) whose `validate_patch_declaration` resolves
   to that rel.
2. Sort the collected patches by `(ordinal, custom_pkg_list_index)` —
   i.e. primarily by the `<NNN>` ordinal parsed from the filename (Prompt
   1), and for ties (same ordinal, different packages) break by the
   package's position in the `-DCUSTOM_PKG=pkg-a,pkg-b` list — the
   earlier-listed package's patch applies first. Do not invent a separate
   ordering file; this reuses the existing, already-explicit `CUSTOM_PKG`
   list order as the tie-break signal, per the project's existing
   "everything declared in meson.build" convention.
3. Apply the sorted patch stack to the same materialized destination file
   in sequence (each patch's `apply_patch` call from Prompt 4 operates on
   the output of the previous one, not fresh against `src/c47/<rel>` each
   time) — this is what makes it *cumulative* rather than last-wins.
4. If any patch in the stack fails to apply cleanly (Prompt 4's checks),
   fail the whole configure step and name the exact patch file and the
   rel it was targeting — do not skip it and continue with the rest of
   the stack.

**Mutation-test requirement:**
- `test_ordering_applies_in_ordinal_order` — two patches, ordinals `010`
  and `020`, both touching the same file but different functions.
  Construct them out of alphabetical/insertion order (e.g. write `020`'s
  file to disk first) and confirm the resolver still applies `010` before
  `020`. Spot-check: swap the sort key to filename-string order instead
  of parsed ordinal, confirm the test now fails on a case where
  `020-...` sorts before `010-...` isn't the actual failure mode to
  chase — instead construct ordinals `9` and `10` (parsed as ints: 9 <
  10, but as strings "10" < "9") to catch a string-vs-int sort bug
  specifically, since `NNN` is zero-padded to 3 digits in the filename
  convention (so `009` vs `010` as strings *does* sort correctly) — use
  this specific case to confirm the implementation truly parses to `int`
  rather than relying on zero-padded string sort accidentally working.
- `test_ordering_tie_break_by_custom_pkg_list_order` — two packages, same
  ordinal, same target file, different (non-conflicting) functions;
  confirm application order follows `CUSTOM_PKG` list order, and confirm
  reversing the `CUSTOM_PKG` list order reverses which one applies first.

---

## Prompt 6 — Same-Function Conflict Handling

**Implements:** `PROPOSED_SPEC_CHANGES.md` §7 (Conflict Philosophy, hard
invariant) and validates it end-to-end through Prompts 4–5's real
machinery — not `git apply` in isolation.

**No new files** — this prompt is a real, end-to-end test plus any fix
needed to make it pass.

**Procedure:**
1. Construct two **scratch** packages (e.g.
   `packages/_scratch_conflict_a/`, `packages/_scratch_conflict_b/`) that
   each generate (via Prompt 3's `refresh`) a patch touching the *same*
   function in the *same* real upstream file, with genuinely overlapping
   edits (not just adjacent lines — actually conflicting content).
2. Configure and build with both packages active:
   `meson setup build.sim --reconfigure -DCUSTOM_PKG=packages/_scratch_conflict_a,packages/_scratch_conflict_b`
   `ninja -C build.sim`
   — i.e. go through the real Meson `custom_target()`/`run_command()` step
   that actually drives this in production, not a standalone script
   invocation.
3. Record the exact outcome: build failure with a clear message naming
   both packages and the conflicting function (PASS), or anything that
   silently picks one package's version with no error (FAIL).
4. If it's a FAIL, this is the concrete bug §7 exists to prevent — fix
   Prompts 4/5's logic so the conflict-marker scan (Prompt 4) actually
   triggers here, then re-run steps 2–3 to confirm PASS.
5. **Clean up:** delete both scratch packages
   (`packages/_scratch_conflict_a/`, `packages/_scratch_conflict_b/`) and
   confirm `git status --porcelain` shows neither afterward. Do not leave
   them committed or in the working tree.

**Report exactly:** command output for the failing build, and an explicit
PASS/FAIL statement for "does a genuine same-function conflict fail
loudly, end-to-end, through the real Meson pipeline."

---

## Prompt 7 — Integration into `custom_pkg_shadow/`, Guard Preservation, and §8 Mutual Exclusivity

**Implements:** wires Prompts 1–5 into `tools/resolve_c47_src.py`'s
existing `do_shadow()` shadow-tree build, plus the ratified §8 rule.

**Files to modify:** `tools/resolve_c47_src.py`, top-level `meson.build`
(to parse and pass through a new `pkg_patch_sources` variable analogous to
the existing `pkg_override_sources` handling at
`meson.build:100-114` — read that block first, mirror its structure for
the new variable rather than inventing a different pattern).

**Requirements (all must hold; do not weaken any existing guard):**
1. The existing sentinel-gate delete-safety check (`SENTINEL_NAME`,
   `assert_shadow_dir`, the F9 wipe-guard) and the containment guards
   (`assert_contained`, F10/F11) in `resolve_c47_src.py` must be
   **unchanged** — you are adding a new overlay source (patches) on top of
   the same shadow tree, not replacing its safety model. If your change
   requires touching any of those functions, STOP and report as
   `[DECISION NEEDED]` rather than proceeding.
2. **§8 mutual exclusivity, enforced here, at configure time:** before
   building the shadow tree, compute the set of rels covered by
   `pkg_override_sources`/`pkg_override_headers` (whole-file, existing
   mechanism) and the set of rels covered by `pkg_patch_sources`
   (function-level, new mechanism), across **all** active packages. If
   any rel appears in both sets, fail immediately with an error naming the
   rel and both contributing packages/files — before any shadow-tree
   mutation happens (i.e. this check must run before the existing F9 wipe
   step, not after).
3. For rels covered only by `pkg_patch_sources`: after the existing
   symlink-the-whole-tree step, apply that rel's sorted patch stack
   (Prompt 5) to produce the shadow-tree copy at `shadow_dir/<rel>` — this
   file is necessarily a real copy (materialized via patch application),
   not a symlink, since it's a derived result; make sure the existing
   "replace existing symlink/file in shadow with the override" logic
   (currently around the `os.path.lexists(dst_path)` handling for
   whole-file overrides) is reused/mirrored correctly for this case too.
4. Extend the existing byte-identical "dead shadow" warning (F15) to also
   cover the degenerate case of a patch stack that nets out to
   byte-identical-to-upstream content (all hunks cancel out or the patch
   is empty) — same warning message pattern, same stderr channel.

**Mutation-test requirement:**
- `test_mutual_exclusivity_rejected` — one package whole-file-overrides
  `keyboard.c`, another patches a function in `keyboard.c`; confirm
  configure fails, naming `keyboard.c` and both packages. Spot-check:
  remove the check, confirm configure now silently succeeds (which file
  wins should be irrelevant to the test — the point is that it doesn't
  fail), restore the check.
- `test_shadow_tree_contains_patched_result` — a single-package,
  single-patch scenario end-to-end: configure with `CUSTOM_PKG` pointing
  at a package with one `pkg_patch_sources` entry, confirm
  `custom_pkg_shadow/<rel>` exists, is a regular file (not a symlink), and
  its content matches applying the patch by hand.
- Existing tests for the whole-file-override path (search for how
  `resolve_c47_src.py`'s current behavior is tested today — likely
  exercised via `da7e42527`/`de3dd7089`/`0eed54ac7`'s test additions, find
  and run them) must still pass unmodified. If you had to change any
  existing test's expected output to make your change pass, STOP — that's
  a sign you weakened existing behavior rather than adding to it; report
  as `[DECISION NEEDED]`.

---

## Prompt 8 — Final Gate and Report

**Run after Prompt 7 is green.** No code changes expected in this prompt
unless the gate below surfaces a regression.

1. Run `meson test -C build.sim` (full suite). Confirm it passes.
2. Confirm the **vanilla** build (no `CUSTOM_PKG`) is still byte-for-byte
   unaffected: configure `build.sim` fresh with `CUSTOM_PKG` unset, build,
   run tests, confirm no diff in behavior from before this branch existed
   — this is the invariant from `design-docs/package-manager/README.md:5-7` and it must
   still hold.
3. Confirm the existing forth-core package
   (`packages/forth-core/meson.build`, whole-file overrides only, no
   patches) still builds and tests clean under the new resolver code —
   this is the regression check for "did Prompt 7 break the mechanism it
   didn't touch."
4. Produce `design-docs/package-manager/IMPLEMENTATION_REPORT.md` listing:
   - Every `[DECISION NEEDED]` raised across Prompts 1–7, with its
     resolution (or, if unresolved, flagged clearly for human follow-up —
     do not resolve it yourself if a prompt told you to stop and report).
   - Every `[GAP]` noted (should be none, per SCOPE LOCK — but report
     honestly if any code you touched revealed one).
   - The real answer from Prompt 4's empirical blob-ancestry check
     (`design-docs/package-manager/PATCH_ANCESTRY_FINDINGS.md`), summarized in 2-3
     sentences here, not re-litigated.
   - Confirmation, with command output, that `meson test -C build.sim`
     passes and the vanilla build is unaffected.
5. Do not commit. Report readiness for human review and stop.
