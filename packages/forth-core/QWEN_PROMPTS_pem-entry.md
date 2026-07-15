# PEM entry fixes — Qwen implementation prompts

Eight tasks, strictly ordered A1→A9 (A5 is merged into A4 — the number is
retired, not reused). Each is sized for a ~100k-token
context window: it names the only files and line ranges to read, and never
requires reading `DESIGN.md` (~2350 lines) or all of `test_dict_reloc.c`
(~5700 lines).

**How to use:** for each task, paste the PREAMBLE below, then the task block,
into a fresh Qwen session. Do not run tasks out of order — later tasks assume
earlier ones landed. Between tasks, a quick human sanity check of the reported
gate output is enough. Commits happen only in A9.

**What this series fixes.** Four defects found on real R47 hardware when
entering FORTH mode from Catalog > FCNS in PEM: (1) the Alpha menu never
appears and the user is stranded in the CAT menu; (2) EXIT destroys alpha input
instead of escaping the menu; (3) the listing shows a redundant `FORTH` prefix;
(4) ENTER drops out of Forth capture with no way back. Every one of these has a
*passing* test today — see A8 for why, and do not trust the existing tests as
evidence of anything in this area.

**Reserved for the architect (not Qwen):** all `DESIGN.md` and
`DESIGN-HISTORY.md` edits. If you believe the spec is wrong, STOP and report —
do not "fix" it.

---

## PREAMBLE (paste at the top of every task)

You are implementing one small, fully specified task in the C47 calculator
firmware repo at `/home/stan/c43`. You are an implementer, not a designer:
follow the spec exactly, make zero design decisions of your own. If an anchor
(a quoted line, function, or search string) does not match what you find in the
file, STOP immediately and report the mismatch instead of guessing.

Rules:
1. First run `git branch --show-current` and confirm you are on
   `forth-core/pem-entry-fixes`. If not, STOP and report.
2. The only build/test command is `./packages/forth-core/build-test.sh` (run
   from the repo root). Success = it prints `FORTH SELF-TEST: ALL PASSED` and
   `==> BUILD + SELF-TEST GREEN.` and exits 0. **Never invoke meson or ninja
   directly** — a hand-rolled build silently omits the self-test suite entirely
   and reports green having asserted nothing.
3. All edits go in `packages/forth-core/`. Never edit anything under `src/`.
   The package has no `meson.build` and nothing to declare — a file mirroring an
   upstream path becomes an override automatically. **But the build reads only
   the GENERATED `patches/`+`files/`, never your edits directly**, so an edit is
   invisible until `python3 tools/pkg_patch_refresh.py packages/forth-core`
   regenerates them. `build-test.sh` runs that refresh as its first step, so
   using the gate is sufficient. If you ever build another way, refresh first or
   you will be testing the previous code while the gate says green.
   Never hand-edit `patches/` or `files/` — they are generated output.
4. Never touch `src/c47/core/freeList.c` or any copy of it. Never read or edit
   `packages/forth-core/DESIGN.md` or `DESIGN-HISTORY.md`. Never read
   `test_dict_reloc.c` in full — read only the line ranges each task lists (use
   `sed -n 'A,Bp'` or `grep -n`).
5. Match the surrounding code style (indentation, brace placement, comment
   density). These are upstream-derived files: keep every line byte-identical
   to upstream except the marked insertion, so the generated patch stays small
   and reviewable.
6. Do not commit unless the task says to. Do not run `git add -A`. **Never run
   `git stash`, `git stash pop`, `git reset`, `git checkout -- <file>`, or
   `git restore`** — not to "get back to clean", not to retry, not for any
   reason. This tree carries uncommitted work from several tasks plus the
   architect's, and `stash@{0}` is often a *foreign* stash from another branch:
   a bare `git stash pop` would dump unrelated WIP on top of your work, and
   `reset`/`checkout --` would silently destroy it. If you think you need to
   undo something, STOP and report — say what you would run and why, and let a
   human do it. A red gate is safe; a mangled tree is not.
7. If the gate goes red on a test that asserts the OLD behaviour your task was
   written to change, that test is part of your task — but **STOP and report
   before touching it**. Never make a test pass by weakening or reverting the
   change it caught; and never "fix" a test the task did not name. A task that
   changes a contract lists every test that encodes it. If one is missing from
   the list, the spec is wrong — say so, and stop.
8. Report at the end: what you changed, the gate output, and anything that
   surprised you.

---

## A1 — Stop `tam.function` being clobbered on every keystroke

**File:** `packages/forth-core/programming/manage.c`

**Read:** lines 1411–1470 only.

**The defect.** `insertStepInProgram`'s first arm is:

```c
  if(func == ITM_AIM || (!tam.mode && getSystemFlag(FLAG_ALPHA))) {
    if(aimBuffer[0] != 0 && !getSystemFlag(FLAG_ALPHA)) {
      pemCloseNumberInput();
      aimBuffer[0] = 0;
    }
    tam.function = ITM_LITERAL;
    pemAlpha(func);
    pemCursorIsZerothStep = false;
    return;
  }
```

(anchor: `tam.function = ITM_LITERAL;` — expect it near line 1426.)

Once a Forth capture is open, `FLAG_ALPHA` is set, so **every** subsequent key
— including ENTER, and including a second `ITM_FORTH` press meant to close the
region — enters through this arm. It sets `tam.function = ITM_LITERAL`,
destroying `tam.function == ITM_FORTH` **before** the `ITM_FORTH` arm further
down is ever reached. This one line silently disables three things that gate on
`tam.function == ITM_FORTH`, and swallows the toggle-off gesture entirely.

This line is inherited verbatim from upstream (`src/c47/programming/manage.c`)
and reads correct in isolation. It is not a typo. Do not "clean it up" beyond
the change specified here.

**The change.** Two edits inside this arm:

1. Exclude `ITM_FORTH` from the condition:

```c
  if(func == ITM_AIM || (!tam.mode && getSystemFlag(FLAG_ALPHA) && func != ITM_FORTH)) {
```

2. Preserve an in-progress Forth capture instead of forcing `ITM_LITERAL`:

```c
    if(tam.function != ITM_FORTH) {   // forth-core: preserve an open Forth capture;
      tam.function = ITM_LITERAL;     // upstream forced ITM_LITERAL unconditionally,
    }                                 // which killed the empty-commit rule, the FWRD
                                      // picker guard and the toggle-off gesture
```

Keep the rest of the arm byte-identical.

**Gate:** `./packages/forth-core/build-test.sh` must stay green. No behavior is
asserted by existing tests here; a green run means you have not regressed
anything.

**Report:** the two edited lines, plus confirmation that
`python3 tools/pkg_patch_refresh.py packages/forth-core` regenerated
`patches/010-programming__manage.c.patch`.

---

## A2 — Full catalog teardown in the `ITM_FORTH` toggle arm

**File:** `packages/forth-core/programming/manage.c`

**Read:** lines 1440–1470 only. Also read
`packages/forth-core/keyboard.c` lines 439–475 and 1205–1220 (read-only — you
are not editing keyboard.c in this task).

**The defect.** The `ITM_FORTH` arm currently does:

```c
    if(catalog) { leaveAsmMode(); popSoftmenu(); }        // as the REM arm
```

(anchor: expect it near line 1449.)

That single `popSoftmenu()` is copied from the `ITM_REM` arm. It removes
`MNU_FCNS` but leaves `MNU_CATALOG` on the stack. Then `pemAlpha` pushes
`-MNU_ALPHA`, control returns to `keyboard.c`, and `_closeCatalog()` runs
**after** `runFunction` (keyboard.c:1213 then :1216). `_closeCatalog` scans the
whole stack for `-MNU_CATALOG`, finds it, and calls `closeAllCatalogMenus()`,
which pops the **current** menu if it appears in `CatalogMenus[]` — and that
array lists `MNU_ALPHA` (keyboard.c:443). The freshly-pushed alpha menu is
eaten and the user is left in alpha *input* with the *catalog* displayed.

**The change.** Tear the catalog stack down to empty **before** `pemAlpha`
runs, so `_closeCatalog`'s `inCatalog` scan finds nothing and it becomes a
no-op. Replace the single line with:

```c
    if(catalog) {                          // forth-core: NOT the REM arm's single
      leaveAsmMode();                      // popSoftmenu(). keyboard.c calls
      while(currentMenu() == -MNU_CATALOG  // _closeCatalog() AFTER runFunction()
         || currentMenu() == -MNU_FCNS     // returns; it pops the current menu if
         || currentMenu() == -MNU_CONST    // it is in CatalogMenus[], which lists
         || currentMenu() == -MNU_CHARS    // MNU_ALPHA — so a half-torn-down stack
         || currentMenu() == -MNU_PROGS    // costs us the alpha menu pemAlpha() is
         || currentMenu() == -MNU_VARS     // about to push. Empty it here instead.
         || currentMenu() == -MNU_MENUS) {
        popSoftmenu();
      }
    }
```

**Do NOT** instead remove `MNU_ALPHA` from `CatalogMenus[]` in keyboard.c. That
array exists to serve the ordinary CAT→ALPHA path and removing it regresses
that path.

**Do NOT** apply the same fix to the `ITM_REM` arm. It has the same latent flaw
and has never shown it, because REM needs no menu after capture opens. Changing
REM is out of scope and would widen the diff for no benefit.

**Also in this task:** confirm the arm derives its toggle direction from
`forthEntryStateAtInsertion()` (not `forthEntryStateAtCursor()`). If it calls
the latter, STOP and report — that is a different defect and not yours to fix.

**Gate:** build-test green.

**Report:** the replaced block, and whether the `wasOn` line already used
`forthEntryStateAtInsertion()`.

---

## A3 — `MNU_FORTH` joins `isAlphaSubmenu`

**File:** `packages/forth-core/softmenus.c`

**Read:** lines 3865–3885 only.

**The defect.** `isAlphaSubmenu` does not list `MNU_FORTH`:

```c
  bool_t isAlphaSubmenu(uint8_t n) {
    return menu(n) == -MNU_MyAlpha ||
            menu(n) == -MNU_ALPHA_OMEGA ||
            menu(n) == -MNU_alpha_omega ||
            menu(n) == -MNU_ALPHAMATH ||
            menu(n) == -MNU_ALPHAMISC ||
            menu(n) == -MNU_ALPHAINTL ||
            menu(n) == -MNU_ALPHAintl;
  }
```

`MNU_FORTH` (the FWRD word picker) is a row on `menu_ALPHA`, so it is reachable
during any alpha capture. `fnKeyExit`'s CM_PEM arm tries `isAlphaSubmenu(0)`
first and pops the menu; if that check fails it falls through to
`pemAlpha(ITM_BACKSPACE)`, which **destroys the capture**. So EXIT from the FWRD
picker currently throws away the line you were typing.

**The change.** Add one disjunct:

```c
            menu(n) == -MNU_ALPHAintl ||
            menu(n) == -MNU_FORTH;      // forth-core: FWRD is an ALPHA submenu —
                                        // EXIT must pop back to ALPHA, not kill
                                        // the capture (fnKeyExit CM_PEM arm)
```

**Gate:** build-test green.

---

## A4 — Bare rendering of Forth source lines (render + EDIT + tests)

> **STATUS: LANDED by the architect (2026-07-14).** Originally split into A4
> (render) and A5 (EDIT). That split was a spec bug: A4 changes a *rendering
> contract*, and both the EDIT extractor and two tests hard-code the old one,
> so A4 alone could never pass its own gate. They are one task. Kept here as
> the record; A5 is now a pointer to this section.

**Files:** `packages/forth-core/programming/decode.c`,
`packages/forth-core/programming/manage.c`,
`packages/forth-core/test_dict_reloc.c`

**Read:** decode.c 820–865; manage.c 782–830 and 560–585;
test_dict_reloc.c 3690–3880 only.

**The defect.** A Forth source step renders `FORTH 'my code here'`. The `FORTH `
prefix is redundant and the quotes are actively harmful: a string-literal step
already renders `'text'` **with** quotes (decode.c:707–713), so quoting the
Forth payload makes the two indistinguishable.

Rendering bare collides with nothing: RPN steps render as `SIN` / `STO 05`, and
a Forth line reading `SIN` renders `SIN` and does the same thing. Where Forth
genuinely differs (`: SQ DUP * ;`, multi-token lines) it renders differently
because it *is* different.

**Change 1 — the render.** In `decodeRem`, the existing `ITM_FORTH` arm handles
`len == 0` (markers) and falls through for `len > 0`. Extend it so `len > 0`
writes the payload text bare and returns, instead of falling through to the
generic `NAME 'payload'` path. The marker arm (anchor: `if (op == ITM_FORTH &&
*(uint8_t *)literalAddress == STRING_LABEL_VARIABLE) {`, expect near line 830)
keeps its current behavior exactly — including the transient-capture exception.
Use `getStringLabelOrVariableName(literalAddress + 1)` then
`stringCopy(tmpString, tmpStringLabelOrVariableName)`.

**Change 2 — EDIT extraction.** The `pemAlpha(ITM_EDIT)` arm has an `ITM_FORTH`
case using offset **8** (assuming `"FORTH"`(5) + `" "`(1) + left-quote(2)) and
stripping a 2-byte right-quote:

```c
      xcopy(aimBuffer, tmpString + 8, ll);
      aimBuffer[ll - 2 - 8] = 0;
```

Bare has no prefix **and no suffix**, so both the `8` and the `- 2` go —
`ll` is `stringByteLength(tmpString)`, so terminating at `ll` keeps the whole
payload:

```c
      xcopy(aimBuffer, tmpString, ll);   // bare render: no name prefix, no quotes
      aimBuffer[ll] = 0;
```

> Dropping only the `8` and keeping the `- 2` silently truncates the last two
> characters of every edited line (`: SQ DUP * ;` → `: SQ DUP *`).

**Change 3 — cursor offset hack.** In `fnPem` (near line 573):

```c
          if(strcmp(tmpString, "FORTH ") == 0) {
            cursorInString = T_cursorPos + 6;
          } else {
```

Now dead, and if it ever matched it would mis-place the cursor by 6 bytes.
Delete the `"FORTH "` branch, keeping the `else` body (the `REM ` / `42` chain)
as the sole path, and preserve the `tmpChar4`/`tmpChar6` save/restore exactly.

**Change 4 — the two tests that encode the old contract.** Both currently pass
*because* the render is prefixed and quoted; changes 1–3 must land with them:

- `test_decode_marker_directions` (~3758) asserts the source step "starts with
  FORTH". Replace with `strcmp(tmpString, ": SQ DUP * ;") == 0`. The three
  marker assertions are unaffected — markers still say `FORTH`.
- `test_decode_source_unchanged` (~3836) exists *to pin the quoting path*; its
  stated escape-mutation is "the new branch swallowing len > 0 steps too" —
  exactly what change 1 does. Its premise is inverted, so rewrite it, don't
  tweak it: rename to `test_decode_source_bare`, assert `tmpString == srcText`,
  and add an explicit check that no `STD_LEFT_SINGLE_QUOTE` /
  `STD_RIGHT_SINGLE_QUOTE` glyph appears (that pins the collision rationale).
  Update the call site (~6206) and both stale PASS strings.

**Gate:** build-test green. It cannot go green until all four changes land.

---

## A5 — (merged into A4)

Was "EDIT extraction offset follows the bare render". It is not separable from
A4 — see the STATUS note there. Skip this number; A6 is next.

---

## A6 — The multi-line lock: ENTER stays in Forth capture

**File:** `packages/forth-core/programming/manage.c`

**Read:** lines 905–1030 only.

**Depends on:** A1 (`tam.function` must survive to be readable here).

**The defect.** `pemAlpha`'s ENTER arm:

```c
    else if(item == ITM_ENTER) {
      pemCloseAlphaInput();
      //--firstDisplayedLocalStepNumber;
      defineFirstDisplayedStep();
        _closeAlphaMenus();
      return;
    }
```

`pemCloseAlphaInput()` clears `FLAG_ALPHA`, and `_closeAlphaMenus()` drops the
alpha menu. The design intended the next printable key to re-open capture — but
`FLAG_ALPHA` is what selects the alpha keyboard layout, so with it cleared,
letter keys produce `ITM_SIN` etc., never `ITM_A`. Only digits could ever
re-open it. There is no keystroke that resumes a Forth **text** line, so the
region is unreachable after the first ENTER.

**The change.** After the commit, re-derive the entry state and re-open a Forth
capture if the cursor is still inside a region:

```c
    else if(item == ITM_ENTER) {
      bool_t wasForth = (tam.function == ITM_FORTH);
      bool_t hadText  = (aimBuffer[0] != 0);           // E5 locks on a NON-EMPTY line
      pemCloseAlphaInput();                            // only: an empty ENTER is the
      //--firstDisplayedLocalStepNumber;               // escape hatch (E3 deletes the
      defineFirstDisplayedStep();                      // placeholder and leaves the
      _closeAlphaMenus();                              // region open behind it).
      if(wasForth && hadText && forthEntryStateAtInsertion()) {   // forth-core: multi-line lock.
        tam.function = ITM_FORTH;                      // State is DERIVED from the
        pemAlpha(0);                                   // program bytes at the cursor —
      }                                                // never stored. ENTER just drops
      return;                                          // to the next Forth line.
    }
```

`pemAlpha(0)` opens a new capture line without feeding a character (item 0 is
not an `addItemToBuffer` item — same mechanism the toggle arm relies on).

**Both snapshots must be taken before `pemCloseAlphaInput()`**: it sets
`tam.function = 0` and clears `aimBuffer` on its way out, so reading either
afterwards gives you the post-commit state, not the decision you need.

**Why `hadText` is load-bearing** (DESIGN.md §8.4 E5: "commits a **non-empty**
Forth line"). E3's empty-ENTER branch deletes the placeholder but leaves the
opening marker standing (manage.c:1000-1009), so the cursor is *still inside an
open region* and `forthEntryStateAtInsertion()` returns **true**. Without the
`hadText` term the lock fires on the escape hatch, re-opens capture, and there
is no way to leave a Forth region except the explicit toggle. `hadText` is what
separates "commit this line, give me the next one" from "I'm done here".

**The test that encodes the old contract.** `test_tam_function_cleared_after_capture`
(~5416) asserts `tam.function != ITM_FORTH` after ENTER — the *pre-E5* contract,
under which the region became unreachable after the first ENTER. E5 inverts it
on the non-empty path. Re-aim it, do not delete it: rename to
`test_forth_multiline_lock_holds`, assert `tam.function == ITM_FORTH` **and**
`FLAG_ALPHA` set, update the call site (~6337) and the PASS string. Its anti-leak
intent moves to the empty path — add a `tam.function == 0` assertion to
`test_forth_empty_enter_leaves_no_step` (~3609), which is the only close path
that must still clear the sentinel.

`forthEntryStateAtInsertion` needs no declaration — it is already prototyped in
`forth_dict.h:148`, which manage.c includes. **Do not add an extern for it.**
(It is declared `bool`, not `bool_t`; a hand-written `extern bool_t …` would be
a conflicting declaration.)

**Gate:** build-test green. It cannot go green until the two tests move with the
code — see rule 7.

---

## A7 — The ALPHA gesture re-opens Forth capture

**File:** `packages/forth-core/programming/manage.c`

**Read:** the arm found by `grep -n "func == ITM_AIM" packages/forth-core/programming/manage.c`
and ~15 lines after it. (It sits near line 1423, but earlier tasks in this
series shift line numbers — trust the grep anchor, not the number.)

**Depends on:** A1.

**The gap.** `ITM_AIM` (the ALPHA key) opens a string-literal capture
unconditionally. Inside a Forth region it must open a **Forth** capture — this
is what makes EXIT's "drop the keypad" level survivable: you drop to the RPN
keypad to press a function key, then press ALPHA to resume typing Forth.
Without it, dropping the keypad inside a region is one-way.

**The change.** In the first arm of `insertStepInProgram` (the one you edited in
A1), replace the `tam.function` guard you added there with the version below.
**Keep A1's explanatory comment** — it records why the guard exists at all, and
the new branch sits in front of it:

```c
    if(func == ITM_AIM && forthEntryStateAtInsertion()) {
      tam.function = ITM_FORTH;         // forth-core: ALPHA inside a Forth region
    }                                   // resumes Forth capture, not a string literal
    else if(tam.function != ITM_FORTH) { // forth-core: preserve an open Forth capture;
      tam.function = ITM_LITERAL;        // upstream forced ITM_LITERAL unconditionally,
    }                                    // which killed the empty-commit rule, the FWRD
                                         // picker guard and the toggle-off gesture
```

`forthEntryStateAtInsertion` is already prototyped in `forth_dict.h:148`, which
manage.c includes — do not add an extern for it.

**Gate:** build-test green.

**Report:** the final shape of the whole arm, so the architect can confirm A1
and A7 composed as intended.

---

## A8 — Tests that drive the real dispatch chain

**File:** `packages/forth-core/test_dict_reloc.c`

**Read:** `grep -n "test_alpha_menu_on_top_during_capture" -A 50` and the
surrounding helper definitions only. Do not read the file in full.

**Why this task exists.** Every defect in A1–A7 shipped with a *passing* test.
`test_alpha_menu_on_top_during_capture` sets `catalog = CATALOG_NONE` and calls
`addStepInProgram(ITM_FORTH)` **directly** — bypassing the `keyboard.c` dispatch
chain (`runFunction` → … → `_closeCatalog`) that is exactly what breaks it. It
asserts `currentMenu() == -MNU_ALPHA` at the one instant that is still true.

**A test that primes the state its subject is supposed to derive proves nothing
about the path that derives it.** That is the rule this task encodes.

**Scope: only the chain matters.** A1, A4 and A6 already have unit-level tests
(`test_decode_source_bare`, `test_forth_empty_enter_leaves_no_step`,
`test_forth_multiline_lock_holds`). Do **not** duplicate them. The gap those
tests cannot close is the *dispatch chain* — every one of them primes state that
`keyboard.c` is supposed to derive. Add only tests that drive the real path.

**Add these three tests**, following the existing file's conventions (static
`test_*` function returning int, `printf("    PASS: …")`, save/restore of every
global it touches, registered in the runner next to the existing alpha-menu
tests):

**Four facts the harness forces on you.** These are verified; do not re-derive
them, and do not work around them by inventing a simulation:

- **`executeFunction` and `_closeCatalog` are file-static** (keyboard.c:969,
  :478) — you cannot call the keyboard's own entry point. `_closeCatalog` is
  therefore exported for this suite via `FORTH_SELFTEST_EXPORT` (keyboard.c,
  under `FORTH_DEBUG_SELFTEST`; production linkage unchanged). Declare it
  `extern void _closeCatalog(void);` and CALL IT. Do **not** hand-roll it as
  `popSoftmenu()` calls: the real one is a no-op once A2 has drained the catalog
  menus, whereas blind pops eat MNU_ALPHA and fail a *correct* implementation.
- **`fnKeyInCatalog` must be set AFTER `showSoftmenu()` and immediately before
  the dispatch** — `showSoftmenu` clears it. Real ordering: keyboard.c:1190
  sets, :1213 dispatches, :1229 clears. Set it too early and runFunction's PEM
  gate (`!catalog || catalog == CATALOG_MVAR || fnKeyInCatalog`) fails, so
  runFunction falls through to `reallyRunFunction()` — *executing* the item
  instead of inserting a step, and the arm under test never runs.
- **The entry state comes from the step immediately BEFORE the insertion
  point** (forth_bridge.c:126: an RPN predecessor ⇒ RPN, regardless of markers
  further back), and **`addStepInProgram` advances the cursor one step before
  inserting** (manage.c:1920-1923). So a fixture that needs "inside a region"
  must leave a marker or a Forth source line as the *predecessor*: use
  `prog[] = { 0x8B, 0x1A, 0xFD, 0x00 }` with `currentStep = beginOfProgramMemory`.
  Parking the cursor after an intervening `ITM_sin` derives RPN — correctly.
- **`ITM_A` is 550.** `'A'` is 65, which is `ITM_EXP`.

1. `test_forth_toggle_from_catalog_leaves_alpha_menu` — set
   `catalog = CATALOG_FCNS`, push the CAT and FCNS menus, set `fnKeyInCatalog`,
   drive `runFunction(ITM_FORTH)` then the real `_closeCatalog()`, and assert
   `currentMenu() == -MNU_ALPHA` **and** `getSystemFlag(FLAG_ALPHA)`. Clear
   `fnKeyInCatalog` afterwards. This is the A2 regression test.
   *Verified mutation:* replace A2's drain loop with upstream's single
   `popSoftmenu()` → `currentMenu()` comes back `-MNU_CATALOG`.
2. `test_forth_capture_survives_keystroke` — the A1 regression test. **Open the
   capture by driving it** (`addStepInProgram(ITM_2)` with the cursor on the
   marker), assert the open succeeded, THEN drive `runFunction(ITM_A)` and
   assert `tam.function == ITM_FORTH`.
   *Verified mutation:* restore the unconditional `tam.function = ITM_LITERAL`.
3. `test_forth_alpha_gesture_resumes_forth` — the A7 regression test; A7 has no
   other coverage. Cursor on the marker, keypad at RPN, **`tam.function = 0`**,
   drive `runFunction(ITM_AIM)`, assert `tam.function == ITM_FORTH`.
   *Verified mutation:* delete A7's ITM_AIM branch → `tam.function` is
   `ITM_LITERAL` (0x0072).

> **Never prime the state the test derives.** Seeding `tam.function = ITM_FORTH`
> in test 3 satisfies A7's `else if(tam.function != ITM_FORTH)` and the test
> passes with A7 deleted — vacuous. Worse, seeding a capture without opening one
> corrupts memory: `pemAlpha` takes the step opcode from `currentStep[0]`
> (manage.c:970), so a cursor parked on `ITM_sin` gets rewritten to
> `4c fd 01 41` — a PTP_NONE item carrying a string. `findNextStep` then steps
> onto the `0xfd`, decodes op `0x7d01`, and `findKey2ndParam` reads
> `indexOfItems[32001]` (LAST_ITEM is 2870). That is a **segfault**, and it is
> what this task exists to prevent.

If driving these paths needs setup beyond the four facts above, STOP and report
what is missing rather than falling back to `addStepInProgram` or simulating a
static function — either would recreate the exact blind spot this task closes.

**Naming.** Do not invent a name that differs from an existing test only by word
order (there is already a `test_forth_empty_enter_leaves_no_step`; a
`test_forth_enter_empty_leaves_no_step` beside it would be a permanent trap).
Grep the file for a candidate name before using it.

**Gate:** build-test green, and all three new tests printing PASS.

---

## A9 — Arena report, then commit

**Depends on:** A1–A8 all landed and green.

1. Run `./packages/forth-core/build-test.sh` once more and capture the full
   gate output.
2. This series changes no dictionary layout, no token encoding and no
   primitive, so the arena high-water mark must be **unchanged**. Report the
   mark from the self-test output and state explicitly that it did not move. If
   it moved, STOP and report — something in A1–A8 touched more than intended.
3. Stage only the files this series touched — working area **and** the
   generated output for each:
   `packages/forth-core/programming/manage.c`,
   `packages/forth-core/programming/decode.c`,
   `packages/forth-core/softmenus.c`,
   `packages/forth-core/test_dict_reloc.c`,
   the regenerated `packages/forth-core/patches/` entries,
   **`packages/forth-core/files/test_dict_reloc.c`**, and
   `packages/forth-core/.refresh-manifest.json`.
   Do **not** `git add -A`.

   > `test_dict_reloc.c` has no upstream counterpart, so refresh classifies it
   > into **`files/`**, not `patches/` — and the build reads only `files/`.
   > Staging the working copy without `files/test_dict_reloc.c` commits a tree
   > whose tests-as-built are the OLD ones: anyone checking out that commit gets
   > a green gate that never runs your changes. Verify before committing:
   > `git diff --cached --stat -- packages/forth-core/files/` must show
   > `test_dict_reloc.c`.

   > `files/test_dict_reloc.c` is the **only** `files/` entry this series
   > touches. `packages/forth-core/.pkgignore` now keeps design docs and
   > `build-test.sh` out of `files/` entirely, so nothing else should appear
   > there. If `git status` shows other `files/` entries added or deleted, that
   > is a package-system change, not this series — leave it unstaged, STOP and
   > report.
4. Commit as a single commit:

```
forth-core: PEM entry fixes — alpha menu, EXIT, multi-line, bare render

Four defects found on real R47 hardware entering FORTH from Catalog > FCNS:

- The ITM_FORTH toggle arm popped only MNU_FCNS, so keyboard.c's
  _closeCatalog() ran afterwards, found MNU_CATALOG still on the stack, and
  popped the alpha menu pemAlpha() had just pushed (MNU_ALPHA is listed in
  CatalogMenus[]). Tear the catalog stack down fully before pemAlpha().
- insertStepInProgram's first arm set tam.function = ITM_LITERAL whenever
  FLAG_ALPHA was set, clobbering an open Forth capture on every keystroke and
  silently disabling the empty-commit rule, the FWRD picker guard and the
  toggle-off gesture. Exclude ITM_FORTH and preserve an open capture.
- ENTER cleared FLAG_ALPHA, which selects the alpha keyboard layout, so no
  keystroke could re-open a Forth text line. ENTER now re-derives entry state
  and stays in capture; ALPHA re-opens capture inside a region.
- Forth source steps rendered `FORTH 'text'`. Quotes collide with string
  literal steps, which already render 'text'. Render bare: a Forth line
  reading SIN now looks like the RPN step SIN, because it does the same thing.

MNU_FORTH joins isAlphaSubmenu so EXIT pops the FWRD picker instead of
destroying the capture.

Tests now drive runFunction with catalog set, rather than calling
addStepInProgram directly with catalog = CATALOG_NONE. Every one of these
defects shipped with a passing test because the tests primed the state their
subject was supposed to derive.

Arena high-water unchanged (no dictionary or token change).
```

**Report:** the commit hash and the gate output.

---

## Next series (not in this file)

- **B — Forth extends RPN: the vocabulary.** `forthFindItem`, §4.1 step 4
  (items resolve before labels), `FTOK_C47` emission, `FTOK_XEQN` for labels.
- **C — Parameterised items.** `STO 05` under C47 convention, `PTP_REGISTER`
  decoder widening.
- **D — Entry-time validation.** Check-only tokenize+resolve on commit.
