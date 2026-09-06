# Audit — program-graphics stages G3 and G4, with the G2 in-family fix wave and the G4 out-of-family fix wave: round 1, at `840fe1c92`

Subject: `0663b2360..840fe1c92` on `program-graphics/stage-g0`, four commits.
`18e222e97` is G3 (the window). `473ed9c4a` is the G2 in-family fix wave.
`e19769236` is G4 (3D) with the G4 out-of-family fix wave folded in.
`840fe1c92` is the README. G0, G1 and G2 sit below the range and appear here
only where a finder reached them.

The projection holds, the byte encoding holds, the window arithmetic holds,
and the five out-of-family fixes hold for the cases their readers named. The
round's yield is elsewhere. **The G4 commit broke the simulator without the
gate noticing**: a search keyed on a stale row comment rewrote upstream's
`"Last item"` sentinel into a second `WIREFRAME` row, and `c47-gtk.c` refuses
to start on it. Behind that, the simulator's own backup would bring the
canvas view back into a `restoreCalc` that has no arm for it. On the device
the first thing a user meets is a 2 KB pool block with no owner after a plot
step, and a still picture that starts rotated or zoomed after `ERASE`. The
test contract is the weakest part of the stage: DESIGN.md §9.8 tabulates 28
pins, 12 have no code, the key-driven redraw has no oracle with content, and
four named mutations leave the gate green. Two of the five folded fixes are
incomplete on the inverse case their own reader named.

After the pass, the operator settled the open suite question of the G4
commit by execution (§4.1): it is an upstream over-read in `parseEquation`'s
label scan, exposed by pool layout, and not a package defect.

> **Filename note.** The dispatched subject string is about 3,900 bytes and
> cannot be a filename (`NAME_MAX` is 255). This file keeps the G2-report
> convention: the name is cut after the tip, and the full subject is stated
> in §1.

---

## 1. Subject and coverage

### The full dispatched subject

Program-graphics stages G3 and G4, round 1: the coordinate window (`XRNG`,
`YRNG`, reals through a window, complex two-point forms), 3D (`EYEPT`,
`XVOL`, `YVOL`, `ZVOL`, `NUMX`, `NUMY`, `WIREFRAME`, `PT3D`, `LINE3D`, the
retained 2 KB pool block, the projection, the keys UP/DOWN/f-UP/f-DOWN/g-UP/
g-DOWN/+/-/5 in the canvas view), the G4 out-of-family fix wave folded into
the G4 commit, and the G2 in-family fix wave (commit `473ed9c4a`: PIXEL flag
protocol outside the view, glyph-walk string cut, arc same-direction arm,
alpha-input prologue, region-2 band clear, `screen.c` function-name arms).
The package lets a user RPN program draw on the screen of the C47/R47
firmware (DM42-class calculator). Source of truth:
`packages/program-graphics/pgmGraphics.c` (the flat working area; `files/`
and `patches/` are generated). Authority: `design-docs/program-graphics/
DESIGN.md` (§5 window, §9 3D, §9.9 and §10 documented limits), TESTING.md §4
(the pins and their mutations), DESIGN-HISTORY.md (G3, G4, and both G2
fix-wave entries).

Out of scope by instruction: G0, G1, G2 code already audited in the G1 and
G2 round-1 reports (`design-docs/program-graphics/audit/AUDIT_G1_round-1_
2026-09-04.md`, `AUDIT_G2_round-1_2026-09-05.md`); their open items are not
re-reported.

Pre-verified facts, operator-checked at the tip and cited rather than
re-proved: (1) `pgArc`/`pgArcPoint` take a user-frame centre row (`cyUser`)
and apply `PG_ROW_OF` at the plot, so `fnGarc` passing `cy` unflipped at line
661 is by contract. (2) DESIGN.md §10 item 10 (every drawing error names X)
and item 11 (range ends equal beyond 34 digits are refused) are documented
limits since `473ed9c4a`. (3) `pg3dReadFloat` rejects NaN and infinity and
`pg3dClamp` bounds every finite coordinate before `pg3dEncode` writes a
`LINE3D` record, so hole byte 255 in a line record has no writer. (4) The G4
out-of-family round raised G-2, G-3, H-1, H-2, I-1, all fixed on the readers'
word with red-first pins and no refutation pass — treated as new code here.
(5) Upstream `saveForUndo`/`undo` copy the stack registers, the system flags,
the stats sums and `lrSelection`; they do not snapshot the pool. (6) The open
suite question (§4.1). (7) Packet H ran at 55 KB, beyond the proven packet
size; its clearances carry that caveat.

### What was read

Eight in-family finders, one per dimension, each blind to the others. All
eight read `pgmGraphics.c` in full (lines 1-2602: the view, the kernel, the
G3 readers `pgRealToPixel`/`pgReadCoordAxis`/`pgReadComplexPoint`/
`pgReadReal`/`pgRange`/`pgReadTwoPoints`, the ten 2D commands, the G2
fix-wave sites, the whole G4 block from `pgReset` to `pg3dKey`, and every
test driver). All eight read DESIGN.md §3, §5, §9, §10 closely, TESTING.md
in full, and the G3, G2-wave and G4 entries of DESIGN-HISTORY.md. All read
the range diff of `keyboard.c`, `screen.c`, `screen.h`, `defines.h`,
`items.c`, `items.h`, `softmenus.c`, `config.c`, `calcMode.c`,
`testSuite.c`, `testSuiteList.txt`, `program_graphics.txt`.

| dimension | beyond the shared set |
|---|---|
| contracts | `keyboard.c` 1604, 1686-1700, 1880-1915, 2136-2160, 2770-2810, 3995-4008, 4025-4080, 4725-4745, 4955-4968, 1305-1316; `screen.c` 2130-2195, 5490-5500, 6228-6245; `config.c` 1540-1565, 1700-1725; upstream `stack.c`, `lblGtoXeq.c`, `solve.c`, `sumprod.c`, `integrate.c`, `items.c` 280-345, `memory.c`, `registers.c` `fnToReal`, `screen.c` `fnPixel`, `equation.c` `_parseWord`, `integrate_cov.txt` head, `saveRestoreBackup.c` `restoreCalc` callers, PTP flags of RESET/DELall/LOAD/LOADST |
| lifecycle | upstream `config.c` `doFnReset` (pool rebuild 1547-1560, `calcModeNormal` 1775, counter zeroing 1898-1900), `setConfirmationMode`, `saveRestoreBackup.c` `saveCalc` 258-333 and `restoreCalc` 822-1010, 1486-1540, `runProgram`/`execProgram` 885-1050, `fnStopProgram`, `engineNestingRefused`, `graph.c` 2118-2135, `error.c` `displayBugScreen`, `keyboardTweak.c`, `statusBar.c` `showHideHourGlass`, `equation.c` 362 and 1041-1050 |
| arithmetic | a scratchpad float harness over `pg3dEncode` at denormal spans, `pg3dRound` near halves, the window scale at float extremes; upstream `registerValueConversions.c` `fnRealToFloat`, `realType.c` `realToInt32C47`, `items.c` `getItemCatalogName`/`getItemFunc` and the tail sentinel, every `LAST_ITEM` user |
| errorpaths | the five out-of-family replies E-I; `keyboard.c` release path 2100-2180; upstream `fnExecute` 167-209, `runProgram`/`execProgram` 891-1045, `covIntegrate`, `CLEAR_KEYS_ON_PGM_START` |
| guards | README.md command tables; upstream `matrix.c` `recallStatsMatrix`, `screen.c` `fnScreenDump` override handling, `c47-gtk.c` main sentinel check, `generateCatalogs.c` loop bound, `assign.c` key rows, `btnPressed` arming and `btnReleased` running, `determineItem`, `resetShiftState`, `covWriteAndLoadPgm` |
| tests | every pin V1-V9, K1-K10, D1-D20, W1-W6, P1-P29, S1, S3, R1-R2, with the arithmetic of each fix-wave pin traced by hand |
| design | README.md; `pgmGraphics.h`; upstream `freeList.c`, `saveRestorePrograms.c` `fnLoadProgram`, `statusBar.c` shift-slot reservations, `screen.c` `showShiftState`, `defines.h` `SBARUPD`/`X_SHIFT`/`Y_SHIFT` |
| upstream | the full `diff -u` of every override against `src/c47` (keyboard.c 16 hunks, screen.c 4, screen.h 1, defines.h 2, config.c 1, items.c 4, items.h 1, softmenus.c 3, calcMode.c 1, addons.c 1) and both testSuite patches; `patch_churn_scan.py` over the twelve patches; `.refresh-manifest.json` of all five packages; every upstream reader of `LAST_ITEM`/`INVALID_MENU`; sibling override lines compared byte for byte (md5) |

Every finding then went to a verifier with one of three lenses
(reachability, correctness, intent), instructed to refute. Every verifier
worktree spawned at `9fdc5753c`, a stale upstream merge that is not an
ancestor of the tip; every verifier ran `git checkout 840fe1c92` before its
first read and reported a clean tree before and after. Twenty of the
twenty-five confirmed entries carry a probe or a mutation that was built,
observed and reverted in the same step; two carry a standalone compile of
the exact code; three are documentary and rest on line-by-line reads.

### What the reading did not reach

The DMCP key-repeat path and `exitKeyWaiting` on hardware. The GTK
`btnClicked` path: no key was pressed in a window, and no window opened
(§3, G34R1-1). The full `fnLoadProgram` body and the full `runProgram` loop.
The equation parser beyond `_parseWord`'s length check and `parseEquation`'s
source pointer. `recallStatsMatrix`'s body. `calcMode.c` and `addons.c`
beyond their package hunks. The bug-classes catalog beyond its first section
(the design finder's read was cut). Compiler warnings were not collected this
round (§2). Firmware size was not re-measured. The G3 boundary walk of Sol's
packet E was accepted as read. The formal three-lens pass did not reach the
seven out-of-family claims listed in §4.2: five of them are the folded fixes,
which every in-family finder cleared independently in its own reading, and
two are covered by pre-verified facts.

### Deliberately not audited

G0, G1 and G2 code except where a finder reached it. The generated `files/`
and `patches/` except the `items.c` patch tail (which is where G34R1-1
lives). The four unfixed out-of-family findings E-1, F-1, F-2, G-1 beyond
the refutation pass. The audit packets as prose.

### Out-of-family accounting

| reader | packet | reply | `MODEL:` line, verbatim | findings raised |
|---|---|---|---|---|
| sol / gpt | `design-docs/program-graphics/audit/PACKET_G3_E_window_sol.md` (430 lines, 22,393 B, axis: the window and its boundary arithmetic) | `design-docs/program-graphics/audit/REPLY_G3_E_sol.md` (197 lines) | `MODEL: GPT-5` | 1 (E-1: long-integer ends that differ beyond 34 digits collapse to equal ends), plus a boundary walk of eight window cases found correct |
| gemini | `design-docs/program-graphics/audit/PACKET_G3_F_contracts_gemini.md` (430 lines, 22,920 B, axis: the callers and the type matrix) | `design-docs/program-graphics/audit/REPLY_G3_F_gemini.md` (36 lines) | `MODEL: Gemini 3.1 Pro (High)` | 2 (F-1: `ARC` centre row unflipped; F-2: type errors name X) |
| sol / gpt | `design-docs/program-graphics/audit/PACKET_G4_G_projection_sol.md` (452 lines, 26,154 B, axis: the projection and the byte encoding) | `design-docs/program-graphics/audit/REPLY_G4_G_sol.md` (141 lines) | `MODEL: GPT-5` | 3 (G-1: hole byte in a retained line; G-2: eps test exclusive; G-3: row clamped before the flip), plus one packet-text note (the `pgWindow` definition appears twice in the packet) |
| gemini | `design-docs/program-graphics/audit/PACKET_G4_H_engine_gemini.md` (1,346 lines, **56,847 B**, axis: the engine protocol and the block lifecycle) | `design-docs/program-graphics/audit/REPLY_G4_H_gemini.md` (29 lines) | `MODEL: Gemini 3.1 Pro (High)` | 2 (H-1: `thereIsSomethingToUndo` re-armed after `fnUndo`; H-2: `ERASE` under `WIREFRAME` leaves a valid 0-by-0 grid) |
| sol / gpt | `design-docs/program-graphics/audit/PACKET_G4_I_commands_sol.md` (334 lines, 21,308 B, axis: the setting commands and the range guards) | `design-docs/program-graphics/audit/REPLY_G4_I_sol.md` (55 lines) | `MODEL: GPT-5.6` | 1 (I-1: a volume span that overflows float), plus the same `pgWindow` packet-text note |

All five replies exist, all five open with a `MODEL:` line, none is empty.
No `.err` file exists in the audit directory for these replies. Two Sol
lines read `GPT-5` and one reads `GPT-5.6`; recorded, not re-dispatched.
**Packet H is 56,847 bytes, 1.5 times the 36.5 KB ceiling PP17 round 7
proved.** Its reply is the shortest of the five (29 lines) and its two
findings were both real; its cleared list (STOP, error, string, nesting,
EXIT, reset, boot paths) is the part that carries the caveat, and §6 marks
which of those clearances an in-family finder re-derived. All five readers
read the G3 or G4 commit; the fix wave folded into `e19769236` is the
repair of G-2, G-3, H-1, H-2, I-1, and no outside reader has read it.

---

## 2. Mechanical results

| check | result |
|---|---|
| `tools/pkg_patch_refresh.py packages/program-graphics` at the tip | idempotent: every verifier that mutated, reverted and refreshed ended with an empty `git status --porcelain` |
| solo gate, `./packages/program-graphics/build-test.sh --solo`, at `840fe1c92` in clean verifier worktrees (run at least nine times as the baseline of a probe) | **GREEN**, `13024 TESTS PASSED SUCCESSFULLY`, 0 failed. `program_graphics.txt` performed: `saddle alone 6083`, `8656 lit pixels in rows 20 to 239`, `122 frames, 6948 program runs` |
| combined gate, `build-test.sh` full run (solo then combined) at the tip | **GREEN** both halves, `1/1 testSuite OK` twice, `PROGRAM-GRAPHICS GATE GREEN` |
| `make pkg_build PKG=packages/program-graphics` at the tip (design verifier) | 13024 passed, 0 failed, exit 0 |
| **GTK simulator at the tip**, `meson setup build.sim ... -DCUSTOM_PKG=packages/program-graphics; ninja -C build.sim src/c47-gtk/c47` (294/294 ok), then `build.sim/src/c47-gtk/c47` and `c47 --headless --exec 'nim 3'` (two verifiers, independently) | **EXIT 1 at `main()` before `gtk_init`**: `The last item (2885)of indexOfItems[] is not "Last item", but is WIREFRAME`. The simulator does not start with this package. G34R1-1 |
| compiler warnings | not collected this round. The G2 report's one package warning (`pgmGraphics.c:557` `-Wsign-compare`) was not re-checked at this tip |
| `design-docs/forth-core/design-audit.sh` (forth-core's drift script, run in the main tree for this report) | exit 0, one finding group: the same two `WS-ONLY` hits in **forth-core's** `010-softmenus.c.patch` (`showKey`/`diagonalsOnTop`, from `b5c4020af`) that the G2 report recorded. Pre-existing, not this package's. No equivalent script exists for program-graphics |
| `patch_churn_scan.py` over the twelve program-graphics patches (upstream finder) | exit 0. Zero `WS-ONLY`, zero `COMMENT-ONLY`. The only NEAR pairs are the real one-token edits §6.9 lists |
| firmware size | not re-measured; the stage commits carry the `make dmcp5r47` deltas |
| tree at the end | every verifier reported `git status --porcelain` empty at `840fe1c92` after its probe (inverse edit, refresh, the suite's `pg3d_*.bmp`/`pg_show*.bmp` frames removed, the suite-regenerated `src/generated/constantsVerification.txt` restored). This pass ran `design-audit.sh` (read-only) and wrote only this file. The main working tree is on `program-graphics/stage-g0` at the tip with unrelated modifications (skill files, forum drafts, the untracked G2 report) that are not this audit's |

Operator corroboration of the simulator row: the main tree's combined
build (`build.sim`, the gate's last pass) exits at once with `The last item
(2885)of indexOfItems[] is not "Last item", but is WIREFRAME`, status 1.

Two harness notes, neither a finding. The testSuite regenerates
`src/generated/constantsVerification.txt` (date, commit, hash lines) on
every run, so a gate run dirties the tree by one file until it is restored.
The 3D showcase writes 122 `pg3d_*.bmp` frames and the 2D showcase two
`pg_show*.bmp` files into the current directory, which is the repo root
under `build-test.sh`; both are untracked and every verifier removed them.

---

## 3. CONFIRMED findings

Twenty-five verifier-confirmed entries collapse to eighteen distinct
findings. Three defects were found by three dimensions each (the pool-block
owner, the driver leak, the missing pins), two by two dimensions (the
sentinel row, the shift-key documentation). Ranked by cost to the owner. A
finding marked **[leave alone]** is one I would not fix if the goal is
correct code; the marks are at the end of §3.

One split verdict is recorded inside G34R1-5 rather than hidden: the same
defect came in twice, once refuted under the intent lens as an inherited
upstream shape, once confirmed under the reachability lens with an executed
reproducer. Both readings are true and the owner decides.

### G34R1-1 — `items.c` rewrites the `"Last item"` sentinel at index 2885 into a second `WIREFRAME` row; the GTK simulator exits at start

- **Where:** `packages/program-graphics/items.c:4809` (second row commented
  `/* 2870 */`, the 1925th and last row of `indexOfItems`, index
  `LAST_ITEM = 2885`); generated `patches/010-items.c.patch:101-102`
  (removes the sentinel, adds the row). Consumer: `src/c47-gtk/c47-gtk.c:
  393-397`, unconditional in `main()` before `gtk_init`.
- **What breaks:** every launch of the simulator built with the package
  prints `The last item (2885)of indexOfItems[] is not "Last item", but is
  WIREFRAME`, calls `readyToExit()` and `exit(1)`. The GTK window and
  `--headless` alike; the check is outside any `#if`. On the firmware there
  is no check, so `indexOfItems[2885]` is a live `CAT_FNCT | SLS_UNCHANGED`
  row where upstream keeps an inert `CAT_NONE | SLS_ENABLED` terminator:
  `getItemCatalogName`/`getItemFunc` (`src/c47/items.c:197, 230`, `abs(itemNr)
  <= LAST_ITEM`) answer `WIREFRAME`/`fnWireframe` for opcode 2885, and the
  suite's `pgmBadOpcode` probe (`testSuite.c:1560-1565`) uses `LAST_ITEM` as
  "a non-item opcode", which is no longer true.
- **Reaching input (executed by two verifiers):** `python3 tools/
  pkg_patch_refresh.py packages/program-graphics; meson setup build.sim
  --buildtype=custom -DRASPBERRY=false -DDECNUMBER_FASTMUL=true
  -DCUSTOM_PKG=packages/program-graphics; ninja -C build.sim src/c47-gtk/
  c47; build.sim/src/c47-gtk/c47 --headless --exec 'nim 3'` → the message,
  exit 1. Plain `./c47` → the same.
- **Origin:** upstream `src/c47/items.c` has two rows commented `/* 2870 */`:
  the CONV spare at 4765 (index 2870) and the sentinel at 4786 (index 2885,
  stale comment). `e19769236` replaced both. `git show <c>:packages/
  program-graphics/items.c | grep -c '"Last item"'` gives 1 at `0663b2360`
  and `18e222e97`, 0 at `e19769236` and `840fe1c92`. The four sibling
  packages keep the sentinel.
- **Why the gate is blind:** `grep "Last item" src/` hits only
  `c47-gtk.c:393`; the testSuite target that `build-test.sh` builds has no
  such check, and every table walk it runs is `< LAST_ITEM` (or finds
  `WIREFRAME` at 2870 first at `testSuite.c:5702`).
- **Violated:** the upstream sentinel contract (`c47-gtk.c:393`, the string
  must read `"Last item"`). DESIGN.md §6 and §9.6.2 (h) claim rows 2864 to
  2872 and nothing else in `items.c`. TESTING.md §6 ("the GTK simulator
  gives a second screenshot ... through the run-sim skill") cannot run at
  this tip; the only picture path that works is the suite's BMP dump.
- **Bug class:** comment-keyed edit over-match (a substitution keyed on a
  stale row comment hit two rows); edit outside the claimed rows.
- **Class-level test:** mirror `c47-gtk.c`'s check into the suite as a pin:
  `strcmp(indexOfItems[LAST_ITEM].itemSoftmenuName, "Last item") == 0` and
  `indexOfItems[LAST_ITEM].func == itemToBeCoded`. Class level: the
  upstream-diff-review scanner carries the package's claimed row ranges
  (2448-2463, 2864-2872) and flags any modified `items.c` row outside them.

### G34R1-2 — the simulator backup restores `calcMode` 21 into a `restoreCalc` that has no arm for it: a bug screen at every launch, and "A restart closes the view" does not hold

- **Where:** `src/c47/saveRestoreBackup.c:1495-1532` (the mode switch, unpatched:
  the package mirrors `keyboard.c`, `screen.c`, `calcMode.c`, `config.c`,
  `softmenus.c`, `items.c`, `defines.h`, `addons.c` for mode 21 and has no
  `saveRestoreBackup.c`); `pgmGraphics.c:57` (`fnPview` sets 21).
- **What breaks:** GTK simulator (PC_BUILD): a program runs `PVIEW` and
  stops, the view stays open. The user closes the window: `gtkGui.c:105`
  calls `saveCalc`, which normalises only `CM_CONFIRMATION`
  (`saveRestoreBackup.c:268-271`) and writes `calcMode = 21` at line 333.
  Next launch: `c47-gtk.c:450` `restoreCalc`; `doFnReset` runs first (line
  831, `pgReset` NULLs the block), then `restoreStateValue(&calcMode)` at
  line 1010 sets 21 again; the switch at 1495-1532 has no arm for 21, so
  line 1531 calls `displayBugScreen("restoreCalc", 21, "calcMode")`.
  `CM_BUG_ON_SCREEN` (`error.c:352-359`) swallows every key (`keyboard.c:
  3102-3105`; `fnKeyExit` has `case CM_BUG_ON_SCREEN: break` at 3807 and
  3881). Closing again saves `CM_BUG_ON_SCREEN`, also unhandled. The bug
  screen returns at every launch until `backup.bin` is deleted. Even with
  an arm added elsewhere, a restored 21 with `canvas.region` 0 (static, not
  saved) clips every drawing command to the one-pixel rectangle (0,0) and
  swallows every key except EXIT.
- **Reaching input:** the sequence above. Read from the code, not executed:
  G34R1-1 stops the simulator before `restoreCalc` runs, so this finding is
  reached the moment G34R1-1 is fixed. Every step is plain control flow in
  unpatched upstream code.
- **Violated:** DESIGN.md §3.3 (line 116): "The struct is not saved in a
  backup. A restart closes the view." The restart does not close the view.
  §9.2 (line 641) records only the pool consequence of `restoreCalc`'s
  `doFnReset` call ("so no second hook is needed"), which is about the 3D
  block, not the mode. §10 item 1 is a device limit about the drawing, not a
  ruling that the simulator may come back in mode 21. Precedent the package
  did not follow: undo-history ships `packages/undo-history/
  saveRestoreBackup.c` with `CM_HIST_BROWSER` added to the `calcModeNormal`
  group at line 1510 (its DESIGN-HISTORY line 165). pretty-print-extra
  (mode 20) shares the gap; the finder's aside that undo-history shares it
  was wrong.
- **Scope:** `saveRestoreBackup.c` is inside `#if defined(PC_BUILD)` (lines
  29-1542). The DM42 device is unaffected.
- **Bug class:** enumerated mode switch missing the registered mode (the
  package mirrored the mode into seven upstream files and not the eighth
  that enumerates `calcMode`).
- **Class-level test:** a pin in the PC suite (the testSuite calls
  `restoreCalc` at `testSuite.c:805, 859`): open the view, `saveCalc` to a
  temp path, `restoreCalc`, assert `calcMode == CM_NORMAL`, `canvas.region
  == 0`, and not `CM_BUG_ON_SCREEN`. Class level: every `switch(calcMode)`
  and every mode group in the mirrored upstream files handles
  `CM_GRAPHICS_CANVAS` (a grep the design audit can carry, listing the
  files that enumerate `calcMode`).

### G34R1-3 — `pg3dEnsure` decides "view open" by `canvas.region`; every other site decides by `calcMode`. After a plot step abandons the view, a 3D command in `CM_NORMAL` takes the 512-block pool block and `LINE3D` retains records

- **Where:** `pgmGraphics.c:877-887` (`if(canvas.region == 0) return true;`
  then `allocC47Blocks(PG3D_BLOCKS)`). Contrast `pgClipNow` (146),
  `fnErase` (71), `pgCloseView` (116), all `calcMode` tests.
- **What breaks:** `canvas.region` has two writers: `pgSetRegion` (35, from
  `PVIEW` and `ERASE`) and `pgCloseView` (121). `pgCloseView` runs only from
  the `CM_GRAPHICS_CANVAS` arm of `fnKeyExit` (`keyboard.c:4001-4003`). A
  plot step writes `calcMode` directly (`fnPlotSQ`, `graphs.c:322`,
  `CM_GRAPH`; `fnPlotStat`, `plotstat.c:1992`, `CM_PLOT_STAT`; the package
  carries no `graphs.c`/`plotstat.c` mirror), and DESIGN.md §3.6 records
  that `canvas.region` stays set after it. EXIT in the plot view goes
  through the `CM_GRAPH`/`CM_PLOT_STAT` arm (`keyboard.c:4029-4063`) to
  `calcModeNormal()`; the package guard (`calcMode.c:44`) fires only when
  `calcMode` is 21, so the mode becomes `CM_NORMAL` with region 6 and
  `pgCloseView` never ran. `EYEPT`, `XVOL`, `PT3D`, `LINE3D`, `WIREFRAME`
  each begin with `pg3dEnsure` and have no `calcMode` test (1229-1281,
  1155); `keyboard.c:2788` blocks items only in mode 21, so they run from
  the CANVAS softmenu.
- **Reaching input (executed by two verifiers, the plot step modelled by
  the exact assignment the plot function makes):** `fnPview(6); calcMode =
  CM_GRAPH; runFunction(ITM_EXIT1)` → `calcMode=0 canvas.region=6
  pg3d.block=(nil)`. `EYEPT 0 -3 0` → pool delta **+512 blocks**,
  `lastErrorCode=0`. `PT3D`, two `LINE3D` → `lineCount=2` in `CM_NORMAL`.
  `pgCloseView` in mode 0 → block still held. `ERASE` then `EXIT` → freed
  (the bound holds: one block, reused).
- **Consequence:** 2 KB of pool held with no view on screen until the user
  happens to run `PVIEW`/`ERASE` and then `EXIT`, or `RESET`. The RCL58
  slack §9.2.4 and §9.9 item 3 budget for the view is consumed in plain
  `CM_NORMAL` work. `LINE3D` outside the view retains records against §9.9
  item 8; the next `PVIEW` empties them, so the visible effect is the
  memory. Pin P3 (2300-2313) tests `EYEPT` outside the view only after
  `pgCloseView` zeroed the region, so this state has no pin.
- **Violated:** DESIGN.md §9.2.4: "the block is allocated by the first 3D
  command that runs while the view is open. Outside the view the 3D commands
  draw once and retain nothing. Reason: only pgCloseView frees the block,
  and pgCloseView runs only from the view. A block allocated outside the
  view has no owner until the next reset." — the region test is what
  allocates it here. §3.6: "canvas.region stays set. Its readers outside
  mode 21 are fnGclip ... and pgRefreshCanvasView" — `pg3dEnsure` is a third
  reader the list does not name. §9.9 item 8.
- **Bug class:** scope-mismatched predicate pair (two predicates for one
  state; one survives a mode change the other does not).
- **Class-level test:** the probe as a pin: `fnPview(6); calcMode = CM_GRAPH;
  runFunction(ITM_EXIT1); EYEPT;` assert `c47MemInBlocks` unchanged and
  `pg3d.block == NULL`; `PT3D; LINE3D;` assert no record. Class level: one
  `pgViewOpen()` predicate, and §3.6's reader list becomes an assertion (a
  grep that no other site reads `canvas.region` for openness).

### G34R1-4 — `ERASE` and `PVIEW` empty the retained content but keep `angX`, `angY`, `angZ`, `zoomStep`; the next still picture starts rotated or zoomed, and the still-picture path applies no z-step threshold

- **Where:** `pgmGraphics.c:41` (`pgSetRegion` → `pg3dEmpty`, 889-893: header
  memset and `haveCur`, nothing else); `pg3dSetup` 977-979 reads
  `pg3d.angX/angY/angZ` and `pg3d.zoomStep`; `pg3dZoomRerun` (1344) has
  exactly three callers, all inside `pg3dKey` (1382, 1383, 1386). The only
  writers that zero the four fields are `pgReset` (853-854) and the `ITM_5`
  arm (1386).
- **Reaching input (executed):** showcase view, `WIREFRAME` of the saddle,
  `+` six times (`zoomStep` 6; the key path re-ran the program with `zRec`
  narrowed to ±0.3146), `ERASE`, `WIREFRAME` again. Result: `zoomStep=6`,
  `frozen=0` after `ERASE`; the second still picture ran the program once
  (576 samples, no re-run) and recorded `zRec=[-1.0, 1.0]` at `zoomStep` 6:
  9,520 lit pixels against 9,991 on the key path, 4,086 canvas bytes
  differ. Variant: 35 `UP`, `ERASE`, `WIREFRAME` → `angX=35`, 2,477 canvas
  bytes differ from the home still picture.
- **Consequence:** the still picture after `ERASE` is encoded over the full
  `ZVOL` range at the zoomed eye: one recorded z step is about 3.0 pixels at
  the near face at zoom 3.81 (the design's own §9.6.6 table gives 1.18 for
  the key path), so the surface is visibly stepped, while the same view
  reached by key presses would have re-run. A still picture can start
  rotated although no sentence says so; README line 79 offers key `5` ("The
  view as set by EYEPT") as the only way back.
- **Violated:** §9.6.6: "A re-run is due when one z step of the recorded
  range covers more than one pixel at the near face" — applied only from
  `pg3dZoomRerun` on a key press. §9.2.4: "ERASE and PVIEW empty the
  retained content. A rotation after ERASE must not bring back a picture the
  user erased" — the content is emptied, the transform that was a view of
  that content is kept. The document's rule of no unstated decision: no
  sentence states what `ERASE` does to the angles and the zoom. The nearest
  pins (P10 at 2337-2346, P19 at 2368-2372) press `5` or return `angX` to 0
  before `ERASE`, so none observes a non-zero transform across `ERASE`.
- **Bug class:** partial reset (one owner's content reset, the sibling
  transform kept; state for one picture split across two structs).
- **Class-level test:** a pin: zoom 6 (or `angX` 35), `ERASE`, `WIREFRAME`,
  then assert whichever rule the owner picks — the transform is zero after
  `ERASE`, or the still picture's `zRec` equals the key path's (the
  threshold applied on the still path too). Either way a pin observes a
  non-zero transform across `ERASE`.

### G34R1-5 — `pg3dEngineEnter` does not test `saveForUndo`'s failure; with a nearly full pool `WIREFRAME` ends with the last sample's values on the stack and no error

- **Where:** `pgmGraphics.c:1111-1128` (`saveForUndo()` at 1113 untested;
  `fnUndo(0)` at 1128); upstream `stack.c:317-323` (`failed:` label clears
  the SAVED registers, `thereIsSomethingToUndo = false`, `lastErrorCode =
  ERROR_RAM_FULL`).
- **Reaching input (executed):** a 64-by-64 real matrix (16,384 blocks, 29 %
  of the 224,976-byte free pool) in X; `WIREFRAME` of any label from the
  keyboard. The row is `US_UNCHANGED` (`items.c:4788`), so
  `reallyRunFunction` runs neither `saveForUndo` nor its RAM_FULL guard
  (`items.c:300-310`); the only `saveForUndo` is the engine's. It fails at
  `copySourceRegisterToDestRegister(X, SAVED_REGISTER_X)` →
  `reallocateRegister` (`registers.c:2095-2101`) → `failed:`. `pg3dRunGrid`
  tests only `ERROR_SOLVER_ABORT` (1078-1081). The first `pg3dSample`'s
  `fnExecute` skips `runProgram` because `lastErrorCode != ERROR_NONE`
  (`lblGtoXeq.c:203-212`); line 1055-1058 reads the stale RAM_FULL as the
  sample's own error, records a hole at (0,0) and clears it; every later
  sample runs; `fnUndo(0)` finds nothing armed. End state from the log:
  `after WIREFRAME ec=0 Xtype=1 Ytype=1 (matrix=6 real=1) undoArmed=0
  lastPointErr=11`, `T=1 (user wrote 4)`. Gate GREEN with that state.
- **Consequence:** after `WIREFRAME` the stack holds the last sample's x, y,
  z with no error shown (one hole at grid point 0,0). The user's matrix is
  gone.
- **Path 2 refuted:** an `UNDO` step in the body cannot exist; `ITM_UNDO` is
  `PTP_DISABLED` (`items.c:3575`).
- **Violated:** DESIGN.md §2 (line 27): "Every command reads the stack and
  does not change it." §9.4.1: "The command restores the stack itself with
  the PLTf pair saveForUndo and fnUndo(0)" — the pair restores only while
  the image is armed; neither the design nor the code names the disarming
  path. Pin P16 covers the armed case only.
- **Split verdict, recorded:** the errorpaths dimension's entry on the same
  path was REFUTED under the intent lens: §9.4.1 carries a DECISION adopting
  the PLTf pair; the §9.4.2 pseudocode writes `saveForUndo()  // solve.c:233`
  with no test; upstream PLTf (`solve.c:234`), SOLVE (123), INTEG
  (`integrate.c:141`), DERIV (`differentiate.c:486`) and the three `graph.c`
  engine entries all leave `saveForUndo` unchecked, so every upstream engine
  has the same hole; the lens proposed one §10 line. The design dimension's
  entry survived under reachability with the reproducer above. Both hold.
  Upstream's own convention for this exact problem exists at
  `items.c:300-310` (`reallyRunFunction`: test `ERROR_RAM_FULL` after
  `saveForUndo`; refuse, or clear and show `TI_UNDO_DISABLED`); the
  upstream-convention-first rule points there rather than at a §10 line.
- **Bug class:** unchecked failure of a state-saving prologue (success-only
  coverage of a two-sided call).
- **Class-level test:** a pin that makes `saveForUndo` fail (the matrix
  above, or a forced-failure hook), then `WIREFRAME`; assert either
  `lastErrorCode == ERROR_RAM_FULL` with X unchanged, or the stack restored.
  Class level: every `saveForUndo` call in the package is followed by a
  `lastErrorCode` test.

### G34R1-6 — I-1 fix incomplete: a volume span narrower than about 7.46e-37 passes `pg3dRange`, and `254.0f / (hi - lo)` overflows to +inf

- **Where:** `pgmGraphics.c:1222-1227` (`pg3dRange`: `!(a < b) || !((b - a)
  - (b - a) == 0.0f)`); `pgmGraphics.c:863-870` (`pg3dEncode`: `t = (v - lo)
  * (254.0f / (hi - lo))`).
- **Reaching input:** in an open view: Y = 0, X = 1e-40 (or 1e-37), `XVOL`
  — accepted (`fnRealToFloat`, `registerValueConversions.c:844-897`,
  returns exactly 1e-40f; not flushed). `PT3D` at x = 0, `LINE3D` at x =
  5e-41. `254.0f / 1e-40f = +inf`; for x = 5e-41, `t = inf` → byte 254 (the
  high face; should be about 127); for x = lo exactly, `t = 0 * inf = NaN`,
  and `(uint8_t)(NaN + 0.5f)` is an undefined conversion (C11 6.3.1.4; 0 in
  practice on x86 and Cortex-M4). Standalone compile of the identical
  `pg3dEncode`/`pg3dClamp`/`pg3dRange` text at -O0 and -O2 (the build has
  no `-ffast-math`): span 7.4e-37 → `254/span = inf`, `enc(hi/2) = 254`,
  `enc(hi/4) = 254`; span 7.6e-37 → 3.34e38, `enc(hi/2) = 127`, `enc(hi/4)
  = 64`. The bound `254 / FLT_MAX ≈ 7.46e-37` is above `FLT_MIN` (1.18e-38),
  so normal floats trigger it and a flush-to-zero FPU does not help.
- **Consequence:** every interior x of the line encodes as the far face. A
  line with both endpoints strictly interior runs along the high face; a
  line from exactly lo to an interior point runs from the low face to the
  high face, twice its length. `fnLine3d` draws from the record (1277), so
  the still line and the retained line are wrong the same way, and a redraw
  reproduces it. No crash; one undefined conversion. No practical volume is
  this narrow.
- **Violated:** §9.2.3: "The values 0 to 254 span the range in 254 steps ...
  A finite value outside the range clamps to 0 or 254" — 5e-41 is inside the
  range and does not clamp. §9.5.2: "the span must be a finite positive
  float" — it is; its reciprocal times 254 is not. §10 and DESIGN-HISTORY
  513-517 record I-1 as the wide-span case only; P20b (2448-2450) pins only
  `XVOL -2e38 2e38`. Sol's I-1 reply named this inverse case in the same
  sentence as the case the wave fixed.
- **Bug class:** one-sided range guard (the span is tested, its derived
  scale is not).
- **Class-level test:** a red-first pin `XVOL 0 1e-37` → refused (guard:
  `254.0f / (b - a)` not finite, or `(b - a) < 254.0f / FLT_MAX`); a P5-style
  encode assertion at the boundary. Class level: every finite-span guard in
  the file also tests each scale derived from the span.

### G34R1-7 — H-2 fix incomplete on the write path: a body that empties and re-freezes the block in one sample keeps `retain` true, and grid bytes are written into a block whose header records no grid

- **Where:** `pgmGraphics.c:1085-1087` (`if(pg3d.block == NULL || h->frozen
  == 0) retain = false;` then `PG3D_GRID()[j * numX + i] = ...`);
  `pgmGraphics.c:1182` (the P29 counts check, which guards the flag only).
- **Reaching input (executed):** `WIREFRAME` of a body `FS?C 00, ERASE,
  PT3D, LINE3D, CLX` on a 17-by-17 grid with flag 00 set. Per sample after
  the first: nothing. First sample: `ERASE` → `pgSetRegion` → `pg3dEmpty`
  zeroes the header (`frozen` 0, `numX` 0, `lineCount` 0); `LINE3D` →
  `pg3dRecordView` (907-931) re-freezes (`frozen` 1) and appends a record
  with `pg3dFreeBytes` computed from `numX` 0. Back at 1085 `frozen` is 1,
  `retain` stays true, and 1087 writes `PG3D_GRID()[j*numX+i]` with the
  caller's `numX` while `h->numX` is 0. Every later sample appends one
  zero-length record. Result: `header numX 0 numY 0 gridValid 0 frozen 1
  lineCount 289; zero-length records corrupted 2 (first 282); grid bytes at
  64..67 = 0 0 0 0`. Arithmetic: sample `idx` writes at `64 + idx` after the
  body appended record `idx` at `2048 - 6*(idx+1)`; collision from `7*idx >=
  1978`, that is samples 283..288 into records 283 (344..349) and 282
  (350..355). The retained lines then decode to wrong endpoints at the next
  key press. With the suite's `ERAS` body (2217) `lineCount` resets each
  sample, one record sits at 2042, and the grid bytes land at 64..67:
  nothing visible, which is why P29 holds and sees nothing.
- **Violated:** §9.9 item 9 / §10 item 20: "A body that calls ERASE or
  PVIEW during WIREFRAME empties the block under the run. The run continues
  and retains nothing." §9.4.3's comment on the same line, "the body emptied
  or reset the block", is the stated intent of a test that stays true.
  §9.2.2: "A later WIREFRAME with another grid size then never moves the
  line records." DESIGN-HISTORY 530 rules only the flag ("a valid grid needs
  the header's counts intact, pin P29"); the test program `ERAS` is even
  commented "a zero-length line that freezes the header again", so the
  re-freeze case was known and the flag was pinned, not the writes.
- **Bug class:** stale local mirror of header state (the flag and the write
  are guarded by different predicates).
- **Class-level test:** the `CERA` body above as a pin: assert every
  retained record intact after the run, or assert `retain` drops when
  `h->numX != numX` inside the loop. Class level: every write into the block
  is gated by the same header-consistency predicate the flag uses.

### G34R1-8 — one stored window, two arithmetic readers: `pg3dSetup` reads `XRNG`/`YRNG` into float with no finiteness guard, so a window the 2D path handles collapses the 3D picture without an error

- **Where:** `pgmGraphics.c:981-983, 988` (`wxs = 399.0f / (float(xmax) -
  wxmin)`, no test); `pgmGraphics.c:1010` (`col = pg3dRound((u - wxmin) *
  wxs)`); `pg3dRound` 993-997 clamps NaN and −inf to −32000, +inf to 32000.
  Contrast `pgRange` (489-509, 39-digit test), `pgRealToPixel` (383-416,
  39-digit map), and the I-1 guard at 1225 for the volume span.
- **Reaching input (executed):** `XRNG 0 1e39` → `lastErrorCode=0 set=1`,
  `wxs=0`, corner (−1,0,0) → col 0, corner (+1,0,0) → col 0; 2D `5e38` →
  col 200 correct. `XRNG 1e30 1.000000001e30` (distinct in 34 digits;
  `fnRealToFloat` keeps at most 9 mantissa digits, so equal in float) →
  accepted, `wxs=inf`, corner (−1,0,0) → col −32000; 2D `1.0000000005e30`
  → col 200. NaN arises only when `u == wxmin` exactly (0 × inf); otherwise
  the product is −inf, and `pg3dRound` sends both to −32000.
- **Consequence:** the whole 3D picture lands on column 0 (or off-screen)
  with no error, while 2D drawing under the same `XRNG` is correct;
  `pg3dZoomRerun`'s `239.0f / s.wys` becomes infinite and its visible-range
  test is meaningless.
- **Violated:** §9.3.4: "The plane coordinates go through the G3 window ...
  as in G3" — the two readers of `pgWindow` disagree beyond float range and
  float resolution. §9.9/§10 carry no item for it (item 11 covers ends
  equal at 34 digits; item 13/24 is the units note). The I-1 fix guarded the
  volume span against the same shape and left the window span unguarded;
  `pg3dReadFloat` guards its own inputs with `f - f != 0` and the window read
  has no guard.
- **Neighbour, refuted (§6.2):** the "24-bit mantissa" precision claim on
  the same lines is the ruled design (PLAN 145-147, §9.3.4). This finding is
  the zero/infinite scale with no error, not the precision.
- **Bug class:** one-sided range guard, second instance; one store, two
  readers with different domains.
- **Class-level test:** pins `XRNG 0 1e39` and `XRNG 1e30 1.000000001e30`
  followed by `PT3D`/`LINE3D`: either an error or a §10 line the pin
  cites. Class level: every `float(r34)` read of `pgWindow` tests its
  derived scale for finiteness, or §10 documents the window's float domain.

### G34R1-9 — a grid above the cap with no lines: §9.2.5 and pin P6 say the next key clears the canvas; §9.6.1 and the code make the key a no-op; P6 is not implemented

- **Where:** `pgmGraphics.c:1370-1374` (`pg3dKey`: `if(h->gridValid == 0 &&
  h->lineCount == 0) return;` before the angle change and before
  `pg3dRedraw`, the only clear); `pgmGraphics.c:1157-1163` (`fnWireframe`:
  `retain = (numX * numY <= pg3dFreeBytes(h))`, header counts left 0 when
  false). DESIGN.md 676-684 (§9.2.5), 1606 (P6), 1192-1212 (§9.6.1 with its
  DECISION).
- **Reaching input:** `PVIEW 6` with a block and no `LINE3D`; `NUMX 45`,
  `NUMY 45` (2,025 bytes against `PG3D_PAYLOAD_BYTES` 1,984); `WIREFRAME` of
  any label → draws once, `gridValid` 0, counts 0. `UP` → returns; the
  over-cap mesh stays on the canvas, no rotation.
- **Violated:** §9.2.5: "The next key press clears the canvas and redraws
  the retained lines only." §9.8 P6: "One UP press then leaves the canvas
  with 0 lit pixels" (expected `0, 0, 0`). §9.6.1: "DECISION: a key press
  with no retained content has no effect", with the same early return in
  its pseudocode. The authority contradicts itself; the code follows the
  DECISION. P6 is absent from the suite and from TESTING.md §4; the
  driver's largest grid is 24 by 24 (lines 2292, 2321, 2351, 2356, 2485), so
  no pin reaches the `retain = false` branch and P6's named mutation
  ("Retain the grid regardless of the cap") could redden nothing. P19's "one
  UP press leaves 0 lit pixels" holds only because `ERASE` already cleared,
  so it does not stand in for P6. §9.2.5's sentence is right when
  `lineCount > 0`.
- **Bug class:** contradicting rules in one authority document;
  specified-but-unimplemented pin.
- **Class-level test:** rule one way; implement P6 as written (45 by 45,
  one `UP`, assert 0 lit or the mesh unchanged). Class level: the
  doc-versus-suite check of G34R1-11.

### G34R1-10 — `pgTestDraw3D` leaks 1,024 pool blocks per run: two `pgReset()` calls with a live block and no free

- **Where:** `pgmGraphics.c:2257-2264` (`pgTestUnitCubeView`: `calcMode =
  CM_NORMAL; pgReset(); fnPview(6); ... fnXvol`), reached with a live block
  from line 2355 (the P29 tail); `pgmGraphics.c:2423` (P27's `pgReset()`
  with the P12 block live). `pgReset` (846-847) sets `pg3d.block = NULL`
  with no `freeC47Blocks`, by design for `doFnReset`, which rebuilds the pool
  (`config.c:1556-1559`).
- **Reaching input (measured by three verifiers):** run the driver.
  `c47MemInBlocks` 2552 at entry → 3613 at exit with `pg3d.block` NULL.
  Block A (`0x..ef48`, allocated by P9's `fnXvol` at 2320, kept through
  `ERASE` because `pg3dEmpty` keeps the allocation) is dropped at 2355:
  the hand-flipped `calcMode` bypasses `pgCloseView`'s guard (116) so
  `pg3dFreeBlock` never runs, `pgReset` NULLs the pointer, `fnXvol`
  allocates block B (`0x..f748`, mem 3102 → 3615). Block B is dropped at
  2423 (mem 3613 → 3613, pointer nil). P20b's `fnXvol` (2449) allocates
  block C, which P18 frees correctly (2461, "the pin returns the block the
  reset forgot"). `pgCloseView` at 2464 frees NULL. Net +1,061 blocks, of
  which 1,024 are the two forgotten blocks and the rest register content
  written by the pins. No test after `program_graphics` in
  `testSuiteList.txt` (positions 461-513) calls `fnReset(CONFIRMED)`, so the
  pool stays 1,024 blocks short for the rest of the process.
- **Consequence:** test-only; every later test file inherits a pool 4 KB
  smaller with a different free-list layout, which the suite's own comment
  (`testSuiteList.txt:456-459`) says the equation coverage files depend on.
  This is the one durable pool-state change the package's suite makes. It
  is relevant to §4.1 and is **not** its mechanism: the leak comes from
  `pgTestDraw3D` alone and does not depend on the undo pair.
- **Violated:** TESTING.md §2 rule 1: "A pin sets the state it asserts and
  restores it." DESIGN.md §9.8: "Every pin sets its state and restores it."
  §9.1.1/§9.2.4 justify "never freed here" only because "doFnReset zeroes
  the pool and rebuilds the free list"; a direct `pgReset()` on a live pool
  has no such cover. P18 states and honours the obligation ("The pin frees
  the block itself after"); the other two sites do not.
- **Bug class:** fixture fakes the state (a hand-flipped mode bypasses the
  guard that owns the free).
- **Class-level test:** a pool-balance pin at the end of `pgTestDraw3D`:
  `c47MemInBlocks` equals its value after the three program loads. Class
  level: every direct `pgReset()` in a test is preceded by `pgCloseView` or
  `freeC47Blocks(pg3d.block, PG3D_BLOCKS)`.

### G34R1-11 — DESIGN.md §9.8 tabulates 28 pins; 12 have no code and P24 is asserted in one of its four counts; the G4 pins that exist bypass `runFunction` and `btnClicked`; four named mutations leave the gate green

- **Where:** DESIGN.md 1590-1637 (§9.8: "Every pin sets its state and
  restores it ... Item drives use runFunction for the commands and
  processKeyAction for the keys, except where a pin names btnClicked",
  then rows P1 to P28); `pgmGraphics.c:2266-2527` (`pgTestDraw3D`,
  `pgTestShowcase3D`); TESTING.md §4 83-110; DESIGN-HISTORY G4 red-first
  table 477-500 (16 rows); commit `e19769236`'s message: "Pins P1 to P26
  as in DESIGN.md §9.8, each with its mutation."
- **Tally at the tip:** present from the table: P1 P2 P3 P5 P9 P10 P12 P16
  P18 P19 P20 P23 P26 P27 P28 (15), plus P20b and P29 (TESTING.md only) and
  S3, R1, R1y, R1z, R2. Absent with no substitute: **P4** (RAM_FULL at
  `pg3dEnsure` and at the rows allocation), **P6** (over-cap grid), **P7**
  (real `UP` through `btnClicked`), **P8** (f then `UP`, g then `UP` through
  `btnClicked`), **P11** (`execProgram` counts across zoom presses; the
  count is printed at 2522, never asserted), **P13** and **P25** (holes,
  all-holes error), **P14** (STOP in the body: `ERROR_SOLVER_ABORT`,
  `gridValid` 0), **P15** (nesting refusal), **P17** (program pointers
  restored), **P21** (`X_SHIFT`/`Y_SHIFT` in mode 21), **P22** (`EYEPT` after
  the freeze). **P24:** S3 asserted (8656), S3a printed (6083) and not
  asserted, S3h and S3b not measured. `btnClicked` has zero hits in
  `pgmGraphics.c`; `runFunction` appears only in the G1 pins (1569-1646);
  every G4 pin calls `fnWireframe(label)` directly and drives keys through
  `processKeyAction`, which is the arm `btnPressed` calls *after*
  `determineItem` (`keyboard.c:1941`), so the shift resolution (1604,
  1691-1700), `resetShiftState` and the release path (2144) are below the
  harness.
- **Mutations executed, all GREEN:** (a) P14's own named mutation ("Skip
  the abort test"): `pgmGraphics.c:1078` forced false; the probe reached the
  generated `files/pgmGraphics.c`; solo and combined gates GREEN — a STOP in
  a `WIREFRAME` body would no longer abort the run and no pin notices. (b)
  P8's named mutation: `keyboard.c:1604` restored to upstream (the package
  clause `(calcMode >= 19 && calcMode <= 23)` removed); the probe reached
  `010-keyboard.c.patch` and the shadow; solo gate GREEN, 122 frames, 6948
  runs. So §9.6.3 (1297-1298) "Pin P8 drives the real path through
  btnClicked, so the solo build is red without (c)" is false at the tip:
  f-UP, f-DOWN, g-UP, g-DOWN are dead from the keys and the gate is green.
  (c) Five simultaneous edits (abort test at 1078, the three pointer
  restores at 1121-1123, the nesting guard at 1154, the frozen eye at 910
  read from `pg3d` instead of the header, and the shift gate): solo gate
  GREEN, 13024 passed.
- **Unpinned arms confirmed by read:** `pg3dEnsure` NULL test 880-882 (P4),
  rows `allocC47Blocks` NULL 1167-1170 (P4 half), `engineNestingRefused`
  1154 (P15), pointer restore 1121-1122 (P17), frozen eye 910 (P22).
- **Violated:** §9.8 as quoted; TESTING.md §2 rule 3: "A pin drives the
  real gesture: the item through runFunction, the key through both key
  paths." The commit message's absolute claim is false for 12 of 26.
  DESIGN.md is the authority and TESTING.md is "the test contract"; the two
  disagree on which pins exist, and no DECISION, PLAN ruling, §9.9/§10
  limit or DESIGN-HISTORY entry defers or drops a pin (the one written pin
  limit, §9.8 1642-1643, covers key repeat and `exitKeyWaiting`).
- **Bug class:** enumeration without a count check; absolute claim never
  executed; harness enters below the layer under test.
- **Class-level test:** a doc-versus-suite consistency check in the gate or
  the design audit: every `| Pn |` row of §9.8 has a `"Pn ` tag in
  `pgmGraphics.c`. Then P7/P8 through `btnClicked` (the run-sim capture
  driver shape), P14/P15/P17/P4 as written.

### G34R1-12 — the 3D redraw has no oracle with content: a `pg3dRedraw` that clears the canvas and paints nothing passes every pin

- **Where:** `pgmGraphics.c:1284-1319` (`pg3dRedraw`, the only painter for
  every key press; `pg3dKey` 1390 is its sole caller). Pins P10 (2340-2346),
  R1/R1y/R1z/R2 (2506-2520), P19 (2369-2373), S3 (2504), P9/P26 (counters).
- **Mutation executed:** lines 1297-1319 (the `gridValid` branch and the
  `lineCount` loop) removed, the `lcd_fill_rect` at 1293 kept. Gate GREEN,
  122 frames written; frame 000 carries 9,635 lit bits, frames 001, 010 and
  121 carry 979, 977 and 979 (9,635 − 979 = 8,656, the S3 count). Every
  rotation, zoom and home key shows a blank canvas and the gate is green.
- **Why every pin passes:** P10 snapshots the canvas after the P9 key
  sequence has already routed through `pg3dRedraw` (blank under the
  mutation) and compares 36 blank redraws with it. The showcase's frame 001
  is a direct `pg3dRedraw()` call (2507), so the snapshot at 2509 is blank
  and R1/R1y/R1z/R2 compare blank with blank. P19 returns from `pg3dKey`
  before the redraw. S3 counts the direct draws of `fnWireframe`, `fnPt3d`,
  `fnLine3d`, `fnGdisp` before any redraw. The absolute oracles DESIGN.md
  §9.7.4 specifies for the redraw — S3h (1569, "The home redraw has its own
  recorded count"), S3b (1571), P22 (1622), P24 (1624) — have no code.
  TESTING.md 104 pins R1/R2 as "the canvas returns after each full turn",
  the periodicity, not the picture.
- **Bug class:** oracle derived from the code under test (redraw compared
  with redraw); TESTING.md §2 rule 2 ("A pin must be red under one named
  mutation").
- **Class-level test:** the S3h absolute count after the first key press,
  recorded once; or a P10 variant that snapshots the still picture from
  `fnWireframe` and compares it with the first redraw at angle 0 — which
  also closes G34R1-13.

### G34R1-13 — the grid sample x and y are computed by two independent copies (still picture and redraw) with no pin across the seam

- **Where:** `pgmGraphics.c:1074, 1076` (`pg3dRunGrid`: `y = ylo + j * ((yhi
  - ylo) / (numY - 1))`, same for x) and `1305, 1307` (`pg3dRedraw`, the same
  two expressions with `view.*` and `h->numX/numY`). Only z goes through the
  bytes (1088-1092, 1309).
- **Mutation executed:** the redraw's x divisor `(numX - 1)` → `numX`. `make
  pkg_build PKG=packages/program-graphics`: 13024 passed, exit 0; every P
  pin, P10, S3, R1, R1y, R1z, R2 green. A probe in the showcase measured
  2,465 of 11,000 compared canvas bytes differing between the still mesh and
  the first redraw. S3 counts the still picture; P10/R1/R1y/R1z/R2 compare a
  redraw with a later redraw; P22 would compare `UP` with `UP`; P2/P6/P13
  count the still picture. No oracle spans the seam.
- **Correction to the finder:** §9.2.3 itself says "The grid x and y are not
  stored" and gives the recompute formula, so the recompute is prescribed
  and the "same decoded values" sentence is about z. The violation is the
  duplicated formula without a pin. §9.2.3's association `xlo + i * (xhi -
  xlo) / (numX - 1)` differs from the code's `xlo + i * ((xhi - xlo) /
  (numX - 1))` by one ulp in 5 of 24 showcase columns (offline check); a
  copy edited to match the document would also go uncaught.
- **Bug class:** duplicated truth.
- **Class-level test:** the still-versus-first-redraw pin of G34R1-12; or
  one `pg3dGridCoord(i, n, lo, hi)` used by both sites.

### G34R1-14 — W3's "a refused XRNG changed the window" assertion is always true: the pixel it reads was lit by W2

- **Where:** `pgmGraphics.c:2145` (`if(!pgTestLit(200,120)) pgTestFail("W3 a
  refused XRNG changed the window")`); W2 at 2126-2137 lights (200,120) with
  the same `LINE` under the same window; no `fnErase` between 2114 and 2140.
- **Mutations executed (three gate runs):** A, the TESTING.md mutation (skip
  the equal-ends check at 496): FAIL lines are "W3 XRNG with equal ends did
  not raise ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN" and "W3 the reversed range
  raised an error" only — the second is the stale `ERROR_OUT_OF_RANGE` from
  the dead check's `LINE` (`pgRealToPixel` divides by zero,
  `realToInt32C47` sets `err`, nothing drawn). B, the defect the assertion
  names (store the window before the check): the "changed the window"
  assertion stays green; only the accidental fourth check reddens. C, B plus
  `fnErase(NOPARAM)` before 2140: "W3 a refused XRNG changed the window"
  fires. DESIGN-HISTORY 319 "No equal-ends check | W3 both checks" names the
  error check and the accidental one; the window check is not among them and
  cannot be.
- **Violated:** TESTING.md §4 G3 row W3 ("equal ends raise ... and leave the
  window"); §2 rule 2 ("A pin that cannot be made red by any mutation is not
  a pin"); DESIGN.md §5.2.
- **Bug class:** assertion on a pre-lit pixel (unfalsifiable pin).
- **Class-level test:** one `fnErase(NOPARAM)` before line 2140 (mutation C
  proves it). Class level: every "unlit" assertion is preceded by a clear or
  an explicit clear-assert, the D8/D13-D16 shape the G2 wave already uses.

### G34R1-15 — the shift-key rule is stale in §3.6 and README.md; §9.9 and §10 carry one limits list twice; §9.6.2 cites a §9.10 that does not exist

- **Where:** DESIGN.md 166 (§3.6: "The shift keys do not engage in mode 21.
  Shifted items are not reachable from the keyboard in the view. Documented
  limit."); `packages/program-graphics/README.md:224` ("Shifted keys do
  nothing in the view.") against README 76-77 ("f UP, f DOWN | Turn about
  the y axis", "g UP, g DOWN | Turn about the z axis"); DESIGN.md 1641-1672
  (§9.9 items 1-15) and 1693-1724 (§10 items 12-26), identical modulo
  numbering (`diff` with the numbers stripped is empty); DESIGN.md 1273
  ("rows 2864 to 2872 (section 9.10)"; `git log -S'### 9.10'` returns
  nothing, the heading never existed; the citation entered in `e19769236`).
- **What the code does:** `keyboard.c:1604`'s gate includes mode 21, so f
  and g engage; the guard arm (2792-2794) routes `ITM_BST/SST/RBR/FLGSV` to
  `pg3dKey`, which steps `angY`/`angZ` (1378-1381); pins at 2323-2326 drive
  those items. The `EXIT` key under f resolves to `ITM_OFF` (`assign.c:42`,
  all layouts), reaches the guard arm and is swallowed (`keyActionProcessed
  = true`, nothing runs): power-off from the view by f-EXIT does nothing
  until the user leaves the view. §9.6.2 (b) documents this swallow as a
  decision; §3.6 says the shift never engages. §9.6.4 records "Section 10
  item 8 changes to: shifted keys engage in the view" — item 8 was changed,
  the §3.6 row was not, and the README then shipped a third stale copy.
- **Violated:** DESIGN.md's writing rule (8-9): "one fact per sentence ...
  A later stage that changes a rule amends this file." DESIGN-HISTORY's G4
  entry (432-435) records the shift change and no §3.6 amendment.
- **Bug class:** rule corrected in a subset of its copies; duplicated truth
  in the authority.
- **Class-level test:** doc-level: one limits list (§10 references §9.9, or
  the reverse); a design-audit grep for "do not engage"/"do nothing in the
  view"; a heading-reference check for `section N.M` citations.

### G34R1-16 — TESTING.md's rewritten D17b row names a mutation site the code no longer has, and the pre-rewrite row still says "None"

- **Where:** TESTING.md 83 ("D17b, rewritten: the canary bytes after the
  NUL survive the trim walk. | Remove the lone-lead-byte stop of the walk.")
  and TESTING.md 70 (the old D17b row: "None ... does not falsify it").
  `pgmGraphics.c:695-703`: the trim walk has no lone-lead-byte stop.
  DESIGN-HISTORY 398-401, same commit `473ed9c4a`: "The trim walk's
  lone-lead-byte stop became unreachable once the boundary cut runs first,
  so it was removed rather than kept as a guard nothing can test; D17b now
  pins the cut through its canary, red under the old cap guard." The pin's
  code comment (2046) names the same real mutation.
- **The pin is falsifiable:** with the old cap guard (`0663b2360` lines
  549-554) in place of `pgGlyphBoundary` (692), `"ABCDEFGH\x80"` (n = 9,
  under the cap) keeps its lone lead byte; the stop-less walk steps from
  index 8 to 10, reads the canary `Y` bytes and writes `tmpString[11] = 0`,
  so "D17b the walk wrote beyond the NUL" fires. The defect is the table
  row, which §2 rule 2 declares to be the mutation list.
- **Bug class:** mutation table drifted from the code it names; stale
  falsifiability claim kept beside its correction.
- **Class-level test:** doc-level: each §4 row's named mutation token
  exists in the source (a grep), and a superseded row is retired, not
  appended to.

### G34R1-17 — the compile-time assert §9.2.2 promises does not exist

- **Where:** DESIGN.md 529: "A compile-time assert pins `sizeof(pg3dHeader_t)
  == 64`." `pgmGraphics.c` has no `_Static_assert`, `static_assert` or
  `sizeof(pg3dHeader_t)`; `PG3D_HEADER_BYTES` (787) and the struct
  (795-806) are tied by hand arithmetic (4 + 2 + 2 + 24 + 8 + 12 + 12), and
  `PG3D_GRID()` (842), `pg3dEmpty` (891) and P12 (2403-2417) use the macro.
  The struct is 64 today (compiled in isolation). forth-core already uses
  `_Static_assert` (`forth_prims.c:295`, `forth_bridge.c:307, 311`,
  `forth_console_view.c:29, 34`), so no toolchain reason stands in for a
  ruling; no ruling exists.
- **Consequence:** nothing today. The next field added to `pg3dHeader_t`
  makes grid byte 0 overlap the header's tail silently.
- **Bug class:** promised invariant not enforced.
- **Class-level test:** the assert itself.

### G34R1-18 — S3's failure message reports the recorded count as 0

- **Where:** `pgmGraphics.c:2504`: `printf("... S3 the showcase count moved
  from the recorded %u\n", (unsigned)0u /* recorded at the first green run
  */)`; the comparison uses 8656; the S1 sibling at 2579 names 10760 in its
  text. Mutation (NUMX 24 → 23) made the gate red with "moved from the
  recorded 0" after "8418 lit pixels". Message only; the assertion fires.
- **Bug class:** message/body mismatch.
- **Class-level test:** one constant used by both the test and the message.

### Which findings I would leave alone

If the goal is correct code and not a clean audit: **G34R1-18** (fix in
passing when the line is next touched). **G34R1-13** is a design choice
the owner can accept as is; the seam pin of G34R1-12 covers it for free.
**G34R1-8** has a legitimate documented-limit disposition on the owner's
own E-1 precedent (one §10 line); the code fix is also one line.
**G34R1-6** the same, though Sol named the case and the guard is one
expression. **G34R1-17** is a one-liner the design already promises.
Everything else is a real defect, a false authority statement, or a pin
that proves nothing, and each of those costs the owner something now.

---

## 4. PLAUSIBLE findings

### 4.1 The open suite question (pre-verified fact 6): unsettled by the readers, settled by the operator after the pass

With `program_graphics` placed before the equation coverage files in
`testSuiteList.txt`, the first formula integration of `integrate_cov.txt`
fails with a syntax error: `_parseWord` under `parseEquation` sees a word
longer than seven glyphs in a formula that reads `X`. It needs both 3D
drivers in one run, goes away without the engine's `saveForUndo`/`fnUndo`
pair, and changes none of 34 probed globals. The list now places
`program_graphics` after `integrate_cov`, which masks it.

**No finder could name the mechanism in the package's code.** All eight
said so explicitly. What they established:

- Every package write into the pool stays inside its own allocation: grid
  index `< numX * numY <= free bytes`, record offset `>= 68` when the free
  test admits it (`2048 - 6*(k+1) >= 64 + grid`), rows index `< 2 * numX`,
  `pgTestCanvasCopy` index `< 12000`. G34R1-7's stray grid writes land
  inside the 2 KB block, not outside it.
- The one durable package-side pool change in the suite is G34R1-10 (1,024
  leaked blocks per `pgTestDraw3D` run) plus the three programs
  `pgTestLoadProgram` loads. The leak shifts the pool layout for every later
  file. It does **not** depend on the undo pair and comes from one driver,
  so it does not by itself match "needs both drivers, goes away without the
  undo pair".
- The undo pair's persistent pool effects are the `SAVED_REGISTER_*`
  reallocations and `savedStatisticalSumsPointer`, the same as any undoable
  operation's; `saveForUndo`'s failure branch (G34R1-5) frees the SAVED
  registers and clears the flag. Under an artificial matrix the probe
  printed `In function saveForUndo: not enough space for saving register
  #100!` — a hint that a RAM_FULL inside the engine, if the suite's pool
  ever gets that tight after the leak, leaves stack registers as sample
  leftovers, which a later test would inherit.
- Ruled out as candidates: `pgCloseView`'s unconditional `pg3dFreeBlock`
  (`freeC47Blocks(NULL, 512)` returns at `memory.c:117-119`, not a bogus free
  region); `_ioFileNameOverride` from the 122 frame names (`fnScreenDump`
  clears it after use, `screen.c:6342`); an in-block overrun (bounded, above).
- `parseEquation` copies the formula from `allFormulae[].pointerToFormulaData`
  (a pool block) into `tmpString` at `equation.c:362`; a token longer than 7
  glyphs from a formula `X` means that block lost its NUL or the formula
  pointer moved. That is a pool-layout question.

**What would settle it:** the operator's AddressSanitizer run, read knowing
G34R1-10 exists. Fix G34R1-10 first and re-run in the original G3 list
position: if the failure moves, the leak was the layout factor. If it
persists, stub the engine's `saveForUndo`/`fnUndo` under a test flag and
bisect which of the two halves (the SAVED reallocations, or `undo`'s copy
back) is needed for the failure.

#### Settled after the pass: an upstream over-read, exposed by pool layout (operator, executed 2026-09-05)

The readers' conclusion above was right on every point it made and wrong on
the one it inferred: the formula block did not lose its terminator and the
pointer did not move. The parser reads past the terminator by design.

Reproduction. A detached worktree at `840fe1c92` with `program_graphics`
moved back to its G3 place in `testSuiteList.txt` (after `regmgmt_cov`),
built with `-Db_sanitize=address`, fails exactly as the G4 entry records:
`integrate_cov.txt` line 13, error 45, 13,023 passed and 1 failed.
AddressSanitizer reports nothing, because the read stays inside the pool
array, which the sanitizer cannot see.

Pool integrity. A throwaway checker in the worktree's `memory.c`, run after
every test function and at every `resizeProgramMemory`, verified that the
free regions and the allocated regions tile the pool below program memory
with no overlap and that the top free region abuts program memory. No
violation in the whole run. There is no double free, no wrong-size free,
and no lost block anywhere in the suite, the package's drivers included.
G34R1-10's two blocks stay in `allocatedMemoryRegions`: a leak by
ownership, not a corruption.

The formula block. A trace in `setEquation` and a liveness check in
`parseEquation` show `"X"` stored at pool block 592, size 1, and that block
live with the recorded size when the parser reads it. A gdb hardware
watchpoint on the four bytes of block 592, armed at the store, recorded no
write before the parse. The text was intact.

The parse. The word reader was entered with

    _parseWord(strPtr = "Syntax error in this equation", parseMode = 1 (XEQ),
               parserHint = 3 (VARIABLE), pointerInFormula = "X")
    bytes at block 592: 58 00 32 2d 70 3a f4 b6 1a b7 0e dc 3a b2 a1 a1

`58 00` is `"X"` and its terminator, `32 2d` are stale padding bytes of the
one-block allocation, and `70 3a ...` is the neighbouring block. Upstream
`parseEquation` (`src/c47/solver/equation.c:1307-1319`) opens with a label
scan that steps up to seven glyphs forward for `':'` or `'('` and never
tests for the terminator:

    for(uint32_t i = 0; i < 7; ++i) {
      strPtr += ((*strPtr) & 0x80) ? 2 : 1;
      if(*strPtr == ':') { labeled = true; ++strPtr; break; }
      else if(*strPtr == '(') { labeled = false; break; }
    }

For the one-glyph formula the scan walks past the terminator, finds `0x3a`
(`':'`) at offset 5, marks the formula as labeled, and parses the
neighbour's bytes from offset 6 as the formula. That garbage raises the
first syntax error at another site; under `OPTION_IR_PRINTING`
(`error.c:305-310`) the error writes its message into `tmpString`, which is
also the parser's token buffer, so the next word-reader call sees the
message as a token longer than seven glyphs. That is the line the log
shows.

What the package contributes is the pool layout, nothing else: the
drivers' allocations, the engine protocol's `SAVED_REGISTER_*`
reallocations, and the G34R1-10 leak decide which bytes sit next to block
592 when `integrate_cov` stores `"X"`. With `program_graphics` after
`integrate_cov` the neighbour holds no `':'` inside the scan and the
defect is silent. The reorder in `e19769236` is a workaround for an
upstream defect, and the comment in `testSuiteList.txt` ("the equation
files depend on the pool state they inherit") records the symptom.

Reachability on the device: any formula shorter than seven glyphs whose
following bytes hold `':'` or `'('` within the scan. `"X"` for the
integrator or the solver is the shortest case. The outcome is a syntax
error at random, or a garbage formula that parses and evaluates. This is
an UPSTREAM finding; it is recorded here because the package's suite
exposed it, an upstream report is the next step, and the class is new in
`references/bug-classes.md` ("a bounded scan with no terminator test reads
the neighbouring pool block"). Bug class for the package side: none; the
package's part is G34R1-10.

Evidence strength: executed (reproduction, pool checker, watchpoint, parser
dump), by the operator alone, no second reader.

### 4.2 Claims beyond the verification cap

Seven out-of-family claims did not receive a three-lens verdict. Their
disposition rests on pre-verified facts and on the in-family finders'
independent reads, each of which cleared the item in its own coverage list.

| claim | disposition | what stands in for the verdict |
|---|---|---|
| F-2 (Gemini G3): type errors name X whichever register held the value | documented limit | pre-verified fact 2 (§10 item 10 since `473ed9c4a`); three finders cleared it |
| G-1 (Sol G4): hole byte 255 in a line record redraws as a phantom line | no writer | pre-verified fact 3 (`pg3dReadFloat` 1190-1203 rejects NaN/inf, `pg3dClamp` bounds before `pg3dEncode` at 1264-1269); four finders re-traced it |
| G-2 (Sol G4, FIXED, P27): eps test exclusive | fix holds, **complete** | `!(dy >= s->eps)` at 1006 is inclusive at eps and rejects NaN; P27 drives both sides (dy = 1.0 = eps passes, 0.5 fails); eight finders cleared it |
| G-3 (Sol G4, FIXED, P28): row clamped before the flip | fix holds, **complete** | the row is clamped after the flip at 1012-1013; the column is clamped inside `pg3dRound` (996) before no flip; the low-side branch `*row < -32000` is unreachable (minimum row is 239 − 32000 = −31761) and matches the §9.3.5 pseudocode; eight finders cleared it |
| H-1 (Gemini G4, FIXED by removal): `thereIsSomethingToUndo` re-armed | fix holds, **complete for the armed case** | no line re-arms the flag after `fnUndo(0)`; `pg3dEngineSave_t` no longer carries it; the image is consumed as after PLTf (`solve.c:248`); recorded in DESIGN-HISTORY 528-529. The disarmed case is G34R1-5 |
| H-2 (Gemini G4, FIXED, P29): `ERASE` under `WIREFRAME` left a valid 0×0 grid | fix holds on the flag, **incomplete on the write path** | `h->frozen && h->numX == numX && h->numY == numY` at 1182 cannot pass after a body `ERASE`; `pg3dRerun` (1325-1342) needs no analogue because `gridValid` is already 0 after any body `ERASE`. The stray grid writes are G34R1-7 |
| I-1 (Sol G4, FIXED, P20b): a span that overflows float | fix holds for the wide span, **incomplete for the narrow span** | `pg3dRange` at 1225 refuses `-2e38..2e38` (inf − inf is NaN, IEEE semantics hold; no `-ffast-math`); the inverse case Sol named is G34R1-6 |

---

## 5. Design observations

1. **The view has two truths for "open".** `calcMode == 21` and
   `canvas.region != 0` are both used as the predicate, and §3.6 already
   documents that they diverge after a plot step. G34R1-3 is the first
   consumer that read the wrong one; the shape guarantees a next one. One
   `pgViewOpen()` ends the class.

2. **The pin table in DESIGN.md is an aspiration and TESTING.md is the
   record, and nothing forces them to agree.** §9.8 was written with the
   design and the drivers shipped a subset; the commit message repeated the
   table. The same shape produced G34R1-9 (a pin that would have decided a
   contradiction was never written) and the false sentence in §9.6.3. A
   count check between the two documents is cheap and would have caught all
   three.

3. **The test harness enters at `processKeyAction`, below `determineItem`
   and `btnReleased`.** That is why the shift gate, the key translation and
   the release path have no pin (G34R1-11 mutation b), and why the G1 K-pins
   also have their own `runFunction(ITM_UP1)` call rather than the key path.
   The run-sim skill's capture driver is the shape that drives `btnClicked`.

4. **The engine protocol copies PLTf, including PLTf's unchecked
   `saveForUndo`.** G34R1-5 is an inherited hole. Upstream's own guard for
   the same problem lives in `reallyRunFunction` (`items.c:300-310`); the
   package's speed law (`US_UNCHANGED`) bypasses exactly that guard. Either
   the guard moves into `pg3dEngineEnter` or §10 says the stack is not
   restored when the pool is full.

5. **Two G4 fixes were one-sided.** I-1 guarded the span and not its
   reciprocal (G34R1-6); H-2 guarded the flag and not the write (G34R1-7).
   Both readers had named the inverse case in their replies. A fix wave that
   lands on the reader's word should pin the sentence the reader wrote, not
   only the case the reader ran.

6. **The 3D rows sit on the head of upstream's CONV growth region.**
   Rows 2864-2872 are a recorded design decision (§6, since G1; refuted as a
   finding in §6.2), and `items.h:2974-2981` tells contributors to add CONV
   pairs incrementally, so the next pair lands on `EYEPT`/`XVOL`. The
   `items.c` hunk (patch line 72, `@@ -4756,15 +4779,15 @@`) then fails
   loudly; `items.h` gets two names for one number (`ITM_2864` and
   `ITM_EYEPT`, lines 3006 and 2528) and merges silently. The tail 2873-2881
   (nine rows) or the plain spare run 2610-2619 (ten rows, unclaimed by any
   sibling) would have sat clear. Recording the reason in §6, or relocating
   before the first upstream merge, is the owner's call.

7. **The freeze-before-run rule is deliberate and under-worded.** A
   `WIREFRAME` whose rows allocation fails still freezes the eye and the
   volume (refuted as a defect in §6.2: §9.2.5's "first record" means the
   `pg3dRecordView` call, and LINE3D past the free bytes freezes the same
   way). README line 81 says "the first 3D drawing", which a user reads as
   "a picture appeared". One clause ("at the first WIREFRAME or LINE3D in
   the view, even one that fails") closes the gap.

8. **`pg3dZoomRerun` is right where §9.6.6's pseudocode is wrong.** The code
   has `if(pps < 0.0f) pps = -pps;` for a mirrored `YRNG`; the pseudocode
   lacks it. Documentation drift in the safe direction; amend the document.

9. **A dead clamp and a dead arm.** `pg3dProject`'s low-side row clamp
   (`*row < -32000`) is unreachable, harmless, and matches the pseudocode.
   With the package's shift-gate clause at `keyboard.c:1604` covering modes
   19-23, the separate `else if(calcMode >= 20 && calcMode <= 23)` arm at
   1691-1700 is unreachable for mode 21 (pretty-print-extra's arm, carried
   byte for byte). Both are the kind of thing the missing P8 would have
   exercised.

10. **Packet size.** Packet H at 56,847 bytes is the largest this project
    has dispatched, 1.5 times the proven ceiling. Its reply was the shortest
    and both its findings were real, but its cleared list is the part a
    reader under load skims. §6 marks which of those clearances an in-family
    finder re-derived; the rest carry the caveat.

11. **The suite-list placement masks rather than settles.** Moving
    `program_graphics` after `integrate_cov` made the open question
    invisible to the gate. §4.1 says what would settle it. Until then the
    package's suite is the one file in the list whose position is chosen to
    avoid a failure rather than to order pool state.

12. **`pgEffectiveCalcMode()` and the manifest base.** The package's
    manifest base `af7ad934a` differs from the four siblings' bases; the
    combined gate composes the stacks and is the owner's check. Not a
    finding; recorded because the next upstream fold will meet it.

---

## 6. Deliberately not flagged

Merged from the eight finders' cleared lists and the seven refuted entries.
Each item says why it is cleared. Items marked **[H]** are clearances Packet
H also gave and an in-family finder re-derived; the caveat of fact 7 does
not attach to them.

### 6.1 The five folded fixes (also §4.2)

- `pgmGraphics.c:1006` (G-2, P27): `!(dy >= s->eps)` inclusive at eps,
  NaN rejected; `ry = 512 − 512 = 0` exactly in the pin, `dy = 1.0 = eps`.
  Complete.
- `pgmGraphics.c:1012-1013` (G-3, P28): row clamped after the flip on both
  sides; col 40000 → 32000, row 239 − (−32000) = 32239 → 32000 (without the
  clamp 32239, as DESIGN-HISTORY records); `pg3dMeshPoint`'s int16 casts at
  1031 are therefore safe. Complete.
- `pgmGraphics.c:1128` (H-1): the restore is gone; the user's pre-WIREFRAME
  undo image is overwritten by `saveForUndo` and consumed by `fnUndo(0)`, as
  after PLTf. Complete for the armed case.
- `pgmGraphics.c:1182` (H-2, P29): a body that `ERASE`s and re-freezes
  fails the counts test; one that `ERASE`s and does not re-freeze flips
  `retain` at 1085; no other body step can rewrite `h->numX` (`WIREFRAME` in
  a body is refused by nesting). The flag is right; the writes are G34R1-7.
- `pgmGraphics.c:1225` (I-1, P20b): `4e38` overflows float to inf, inf − inf
  is NaN, refused; with ends near ±1e38 the later float arithmetic in
  `pg3dSetup` and `pg3dEncode` stays finite. The narrow span is G34R1-6.

### 6.2 Refuted claims

- **`pg3dKey` → `pg3dRedraw` NULL dereference after a body reset**
  (`pgmGraphics.c:1297`). Real code shape (`pg3dRedraw` takes `PG3D_HDR()`
  without a block test while `fnWireframe` 1178 and `pg3dRerun` 1337
  re-fetch), unreached by any input. `pg3d.block = NULL` has two writers:
  `pgReset` (one caller, `doFnReset`, whose callers are `fnReset` — RESET is
  `PTP_DISABLED`, `items.c:3443` — `restoreCalc` at startup/suite end, and
  boot) and `pg3dFreeBlock` (one caller, `pgCloseView`, reached only from
  `fnKeyExit`'s mode-21 arm; EXIT is `PTP_DISABLED`, `items.c:3612`). A
  `PTP_DISABLED` step never runs its function (`lblGtoXeq.c:830-836` raises
  `ERROR_NON_PROGRAMMABLE_COMMAND`; the loader drops such steps,
  `saveRestorePrograms.c:120`). The run loop never dispatches a key to
  `fnKeyExit`: on DMCP it polls `C47PopKeyNoBuffer` and sets `PGM_WAITING`
  (`lblGtoXeq.c:965-985`); on PC `exitKeyWaiting` reads `currentKeyCode`
  (`addons.c:1113-1131`) and the GTK loop is blocked under the synchronous
  `execProgram`. `INPUT`/`PSE` pop keys directly. `fnKeyUp`/`fnKeyDown` are
  `PTP_DISABLED` too, so a body cannot re-enter `pg3dKey`. **[H]**
- **The 3D window read into float (24-bit mantissa)** (`pgmGraphics.c:979`).
  The ruled design: PLAN 145-147 ("The 3D math runs in float. The processor
  of the DM42 has hardware for float only"); DESIGN.md §9.3.4 spells out
  `w->xs = 399.0f / (float(pgWindow.xmax) - w->xmin)` and names the
  conversion; §9.9 item 13 / §10 item 24 give the operating rule ("A 3D
  drawing needs XRNG and YRNG in volume units, or the eye and the volume in
  pixel units"); §5.2's 39-digit clause governs the 2D conversion, a
  separate documented path; Sol's packet-I reply read the contract the same
  way. `XVOL` ends equal in float are refused with the documented error. The
  inf/zero-scale case with no error is G34R1-8, a different claim.
- **`saveForUndo` failure swallowed by the body's `runProgram`**
  (`pgmGraphics.c:1109`), intent lens: the unchecked call is the shape
  §9.4.1's DECISION adopted (the PLTf pair, `solve.c:233/248`); the §9.4.2
  pseudocode writes it without a test; every upstream engine does the same.
  Refuted as a design omission. The same path is CONFIRMED as a reachable
  wrong result in G34R1-5, where the split is recorded.
- **`WIREFRAME` freezes the eye before it runs; a failed run leaves a frozen
  empty block** (`pgmGraphics.c:1156`). The §9.4.2 pseudocode has the freeze
  at step 3 before the rows allocation at step 4 and the run at step 5, and
  the code follows it; §9.2.5 defines `pg3dRecordView` as "called by
  WIREFRAME and by LINE3D before a record" and sets `frozen = 1` on that
  call, with a DECISION ("this rule keeps the still picture and every redraw
  consistent"); §9.5.4 freezes a `LINE3D` past the free bytes the same way;
  P22 pins the freeze; Sol cleared `fnEyept` after freezing "matching the
  freeze contract". Wording gap in README 81 recorded in §5 item 7.
- **3D rows 2864-2872 on the CONV growth region** (`items.c:4782`). A
  recorded decision since G1 (`cb2ae56e7`: "the 3D rows from 2864 on when
  G4 lands"), labelled "upstream CONV spares" in the §6 row table, repeated
  in DESIGN-HISTORY 407 and the G4 commit; filling `CAT_FREE` rows is the
  sanctioned mechanism (exceptions catalog line 18) and no rule picks the
  rows. The hazard is real and is §5 item 6. This refutation's side
  observation is where G34R1-1 was first seen.
- **E-1 (Sol, G3): long-integer ends that differ beyond 34 digits collapse
  to equal ends.** Constructible (`10^39` and `10^39 + 10^5` both become
  `1.000...0E+39` at `pgReadReal` 470-471; `pgRange` finds a zero difference
  and refuses with the old window kept) and the specified behaviour: §5.2
  347-349 ("two ends that differ only beyond 34 digits are equal ends. Audit
  G3 round 1, Sol 1"), §10 item 11, DESIGN-HISTORY 371-375, pin W3. The
  refusal is the safe arm: a 39-digit compare would accept and store two
  equal real34 ends, and `pgRealToPixel` would then divide by zero.
- **F-1 (Gemini, G3): `fnGarc` passes `cy` unflipped.** By contract
  (pre-verified fact 1): `pgArc`'s parameter is `cyUser`; the flip is applied
  at the plot in `pgArcPoint` (330) and the `r == 0` arm (339) because
  `pgInSpan` (314-322) tests the direction in the math frame with y upward;
  a flip before the call would flip twice, which is the defect the finding
  described. D16 (2012-2029) asserts user-frame pixels (300,130) and
  (300,133) through `pgTestLit`, which applies `PG_ROW_OF`; a mirrored arc
  would fail it.

### 6.3 The G3 window and the readers

- `pgRealToPixel` (383-415): NaN/inf refused first; half away from zero via
  `realToInt32C47`'s `DEC_ROUND_DOWN` on the adjusted value, correct for both
  signs; the ±32767 test after rounding, symmetric (−32768 refused); int32
  overflow via `err`; `PG_ROW_OF` of 32767 stays in int32; matches §5.2 and
  upstream `screenWindowRatio`. Sol's eight-case boundary walk agrees.
- `pgRange` (489-509): reads Y then X, equal ends detected on the same
  real34→real39 difference that `pgRealToPixel` later divides by, so the
  divide by zero cannot arise; NaN/inf ends refused in `pgReadReal`; the
  window written only after both reads and the test pass; the axis bit set
  last. A long-integer end of up to 1,001 digits fits real34 (emax 6144), so
  no infinite end enters the window.
- `pgReadTwoPoints` (555-574): both or neither complex, first in Y, second
  in X; a complex in one of X and Y without the other is the type error
  before any read; Z and T complex in the four-real form fall to the type
  error; Z and T untouched in the complex form; matches §5.1.
- `pgReadCoordAxis`/`pgError` (371-373): every error names X, §10 item 10
  (fact 2).

### 6.4 The 3D block, the encoding, the projection

- `pg3dFreeBytes` (858-861): guarded against underflow; the grid grows from
  64 up and the lines from 2048 down; `free >= 6` before a record gives
  `2048 − 6(L+1) >= 64 + numX*numY`, so no overlap while the header is
  consistent; after a body `ERASE` both are garbage and `gridValid` is 0
  (the consistent-header case; the inconsistent one is G34R1-7).
- `pg3dEncode`/`pg3dDecode` (863-875): `t < 254` gives at most 254 after the
  +0.5 cast; 255 is only the NaN/inf arm; P5 values hold; hole byte 255 in a
  line record has no writer (fact 3).
- `pg3dRound`/`pg3dProject`: the ±32000 clamp before and after the row flip
  keeps every value in int16 and away from `PG3D_NOPIX`; the float half-up
  band of about ±0.002 near an exact half is the documented "pins avoid
  halves" case.
- `pg3dMeshPoint` (1031): `rows` holds `2 * numX` entries of 4 bytes,
  indexed `i < numX` in both halves; `prev` read only for `j > 0`;
  `PG3D_NOPIX` marks the missing sample before the early return;
  `pg3dRedraw` allocates by `h->numX`, at most 44 when `gridValid`.
- `pg3dRecordView` (907-930): the eye rule refuses before it freezes; the
  frozen header wins; matches §9.2.5 line for line.
- `pg3dMatrix` and the 36-entry table (940-975): `M = Rz * Ry * Rx`; integer
  step counts modulo 36; `(angX + 35) % 36` promotes to int before the
  modulo; P1's eight corners match §9.3.6 by hand.
- `pg3dRedraw` and `pg3dZoomRerun` ignore `pg3dRecordView`'s return:
  reached only when `gridValid` or `lineCount` is nonzero, both set only
  after a successful `pg3dRecordView` on the same header, so the failing
  branch is unreached. `pg3dRedraw`'s clear covers rows 20 to the region
  bottom from `canvas.region`, correct because `pg3dKey` runs in mode 21
  only; the mode is restored.
- `pg3dZoomRerun` (1344-1371): `gridValid` 0 or label 0 returns (label 0 is
  below `FIRST_LABEL`); `|pps|` covers a mirrored `YRNG`; NaN scales fall
  through to the return; `zNewLo < zNewHi` guards the re-run; the
  `pg3dZoom` index stays in 0..16 because the key tests the bound first.
- `pg3dKey` (1370-1391): no block or nothing retained means no state change
  (K2 stays green); zoom presses clamp at ±8 before the re-run; key 5
  returns early when already home; `ITM_4`/`ITM_6` fall to `default`.
- `pg3dReadCount` (1209-1220): a NaN or huge real sets `err` or fails the
  2..100 test before the integer test; 2.5 fails `realIsAnInteger` (P20).
- `pg3dReadFloat`/`pg3dReadPoint`/`pg3dRange`: short-circuit reads write only
  locals; the target fields change only after every read passed; the volume
  centre and eye-distance sums overflow only at `|v| > 1.7e38`, which the
  span test already refuses in the common case; a span finite but whose
  midpoint sum overflows (1e38 to 3e38) draws nothing without an error —
  accepted as absurd input.
- `fnLine3d` (1245-1281): a failed `pg3dRecordView` leaves the current point
  and `lineCount` unchanged; the record is written only when `free >= 6`; the
  current point keeps the unclamped values; the still line and the redraw use
  the same six bytes; P12's 330 × 6 = 1980 ≤ 1984 and the 331st at offset 62
  would overlap header bytes 62-63 under the mutation, `memcmp` fires.
- `fnWireframe` (1131-1189): the label check (1141-1153) is byte-for-byte
  `sumprod.c`'s `_checkArgument`, local labels 00-99 refused with
  `ERROR_OUT_OF_RANGE` as the sum engine refuses them; the old grid is
  dropped before the rows allocation can fail ("gone in any case", §9.4.2
  step 4, a decision); rows freed with the same size expression and only
  when `pg3dResetCount` is unchanged; the three program pointers restored on
  both arms; the ALLHOLES error re-raise works. `pgClipNow` taken once
  before the run (1174): a body `PVIEW 2` leaves a stale region-6 clip and
  rows 171-239 are repainted by `pgRefreshCanvasView`, covered by §9.9 item
  9. A body that runs `PVIEW` while the view was closed at entry keeps the
  whole-screen clip captured at entry: cosmetic, not flagged.
- `pg3dRunGrid` (1071-1098): `numX, numY >= 2` by `pg3dReadCount` and the
  header copy, so the `(n − 1)` divisions are safe; `retain` once false stays
  false (only ever assigned false); the stale `h` after a body reset is never
  dereferenced because `pg3d.block == NULL` short-circuits; the abort test
  matches `differentiate.c`'s shape and the post-loop test covers an abort in
  the last sample. A STOP in the very last sample is not caught
  (`PGM_WAITING` leaks to the caller and the grid is marked valid); this
  matches the §9.4.3 pseudocode line for line and the picture is complete.
  **[H]**
- `pg3dSample` (1044-1058): clears every error but `ERROR_SOLVER_ABORT`, as
  the design says; `runProgram` resets `lastErrorCode` at entry, so no stale
  code crosses samples once the first has run (the first sample's stale
  RAM_FULL is G34R1-5).
- The header struct is 64 bytes today (G34R1-17 is about the missing
  assert).

### 6.5 The engine protocol and the lifecycle

- `pg3dEngineEnter`/`Leave` order (1107-1129): the nesting guard before
  `saveForUndo` as `solve.c`'s comment demands; `saveForUndo` before
  `setSystemFlag(FLAG_SOLVING)`; `clearSystemFlag` before `fnUndo` so
  `undo()`'s re-flip is a no-op (`stack.c:339-420`); `temporaryInformation`
  cleared before the undo; counters decremented in enter order.
  `saveForUndo`'s `CM_NIM/AIM/MIM/CM_NO_UNDO` early return does not fire in
  mode 21 or `CM_NORMAL`; `CM_NO_UNDO`'s only setter is `graph.c:2127`
  inside the complex solver, whose body cannot reach `WIREFRAME` (refused at
  depth 1). **[H]**
- `engineNestingWasRefused` read at 1079: `WIREFRAME`'s own
  `engineNestingRefused(true)` at entry clears it, so the read is fresh;
  matches `graph.c:1213-1217`. A nested engine refused inside the body sets
  `thereIsSomethingToUndo = false` (`solve.c:50`), so the outer `fnUndo(0)`
  restores nothing after that abort: upstream's stated decision in the
  `engineNestingRefused` comment, SOLVE and PLTf behave the same, and the
  program halts with an error anyway. Noted as a gap in §9.4.1's
  unconditional "restores the stack itself", not flagged. **[H]**
- `programRunStop` after a keyboard `WIREFRAME`: `runProgram`'s
  `nestedEngine` is false when entered from `PGM_STOPPED`, so `END` sets
  `PGM_STOPPED` after the first sample; `execProgram`'s second `runProgram`
  is skipped; the state is as §9.4.2 says. `FLAG_SOLVING` keeps `runProgram`
  from touching `screenUpdatingMode`, `showHideHourGlass` and
  `refreshScreen(4)`, so the view's manual flags and pixels survive each
  sample; the stale `hourGlassIconEnabled` is not visible because
  `showHideHourGlass` paints by `programRunStop`.
- STOP in the body: `fnStopProgram` sets `PGM_WAITING` only; the abort test
  (1079-1083) converts that to `ERROR_SOLVER_ABORT` and `PG3D_RUN_ABORTED`,
  so §9.4.2's "abort sets PGM_WAITING" holds; from a key-press re-run the
  grid is dropped and the lines kept, as §9.6.6 rules; the error paints on
  canvas line 1 at the next refresh. **[H]**
- `pg3dRerun` from a key (1331): `engineNestingRefused(true)` from the
  keyboard with depth 0 never refuses; enter/leave are balanced on every
  path; `CLEAR_KEYS_ON_PGM_START` is 0, so the zoom re-run inside
  `processKeyAction` does not drain the key buffer and the release event
  survives.
- RESET inside a body: `items.c` row 1568 is `PTP_DISABLED`, so RESET is not
  a program step; `fnReset(NOT_CONFIRMED)` only enters `CM_CONFIRMATION` and
  `doFnReset(CONFIRMED)` runs from the YES key; `doFnReset` cannot run
  mid-body. The engine-counter zeroing at `config.c:1898-1900` under a live
  `pg3dEngineLeave` would wrap to 65535 and refuse every later engine, but
  the path is UNREACHED, and the design's `pg3dResetCount` guard protects the
  same unreachable path. **[H]**
- `pgReset` (846-856): no free, block NULL, HP defaults, `pgWindow.set = 0`,
  canvas untouched; correct for its one designed caller `doFnReset`, which
  rebuilds the pool (`config.c:1547-1560`) and runs `pgReset` 16 lines after
  `histElementXorY = -1` as §9.1.1 anchors it; P18 pins the no-free.
  `doFnReset` in the view calls `calcModeNormal` (1775), which the package
  guard returns from in mode 21, so the view stays open with a blank canvas
  as §10 item 7 says.
- `restoreCalc` ordering: `doFnReset` runs before the values are restored
  (`saveRestoreBackup.c:831`), so `pgReset` NULLs the block before the pool
  image is rebuilt; the block is not restored and not leaked on that path.
  (The mode restore is G34R1-2.) **[H]**
- `pgCloseView` (115-133): frees the block through `pg3dFreeBlock` before
  the mode restore (`freeC47Blocks(NULL, 512)` returns at `memory.c:117`, not
  a bogus free region), clears `haveCur`, restores `FLAG_ALPHA` and
  `cursorEnabled` for `CM_AIM` after the refresh; matches §3.6 and the K8
  fix. P3 proves the free on the real EXIT path.
- `fnPview` (45-68) and `pgSetRegion` (34-42): the alpha prologue runs
  before `pgSetRegion`; a second `PVIEW` keeps `prevCalcMode` and does not
  take the cursor twice; `ERASE` and `PVIEW` both go through `pgSetRegion`
  and both call `pg3dEmpty`, pinned by P19.
- `calcModeNormal` guard (`calcMode.c:44`): tests `calcMode == 21`, not the
  region, so EXIT from a plot mode still returns to `CM_NORMAL` (the region
  it leaves behind is G34R1-3).
- Label code identity across program deletion (`h->label` at 1183, read at
  1329): deletion needs PEM, PEM needs EXIT, EXIT frees the block;
  unreachable while a block lives.
- `_ioFileNameOverride`: `fnScreenDump` clears it after use (`screen.c:
  6342`), so the 122 frame names cannot redirect a later file load.
  `pgTestLoadProgram` matches `covWriteAndLoadPgm` byte for byte, clears
  `aimBuffer` and removes the file; its early return on an existing label is
  intentional and a failed load surfaces as `ERROR_OUT_OF_RANGE` in P2.

### 6.6 The keys and the screen arms

- The guard arm (`keyboard.c:2788-2797`): the item list matches §9.6.1;
  `ITM_RS` still sets `showFunctionNameItem = 0`; `ITM_OFF` and `ITM_PR` stay
  swallowed as §9.6.2 (b) says (the stale §3.6 row is G34R1-15); the press
  sets `keyActionProcessed`, so `btnPressed` (1948) never arms the item and
  the release runs nothing; `+` and `−` zoom without running `fnAdd`/`fnSub`;
  the UP/DOWN cases (4737, 4961) set `keyActionProcessed` before
  `fnKeyUp`/`fnKeyDown`, so one press is one step; the K2 pin's
  `runFunction(ITM_UP1)` is the pin's own extra call, not the key path.
- The shift gate (1604): byte-identical to undo-history's line (md5);
  `resetShiftState` at 1730 runs for every result except SNAP, so no shift
  sticks (P8's "shiftF is false after" premise holds by code); f-ENTER
  (`KEY_COMPLEX`), g-ENTER, f-BACKSPACE (`UNDO`), g-BACKSPACE, f-R/S (`PR`),
  g-R/S all fall to the guard arm and are swallowed; g-EXIT is SNAP and runs
  as documented; CC/op_j/op_j_pol arm and release into item functions with a
  mode-21 no-op case.
- The shift glyph in mode 21: `commonShiftProcessing` → `showShiftState`
  paints at `(X_SHIFT_R, 0)` through the two `defines.h` macros;
  `statusBar.c:863` (`Y_SHIFT == 0 && X_SHIFT > 300`) reserves the right slot
  from the same macros, so the glyph and the slot agree; `clearShiftState`
  clears the same slot while `calcMode` is 21; no path leaves mode 21 with a
  shift engaged; `clear_fg_jm` returns unless `FLAG_FGLNFUL/FGLNLIM`
  (documented limit 12/23). The macros' fall-through for other modes is
  unchanged (`statusBar.c:120, 829, 863`, `manage.c:453`).
- `showFunctionName`/`hideFunctionName` arms (`screen.c:2139, 2179`): the
  item and counter are still recorded at the press; the release path copies
  `showFunctionNameItem` at `keyboard.c:2161` before `hideFunctionName` at
  2178, and upstream's own tail zeroes the item too; EXIT still runs on
  release; no timer start sits after the early return; K9 drives both.
- `pgRefreshMaybe` outside the view (535-552): the same three manual flags
  and `screenHoldsDrawnPixels` as upstream `fnPixel` (`screen.c:6556-6557`),
  set after a successful draw only; `fnPixel`'s extra
  `SCRUPD_MANUAL_STATUSBAR` for rows at or below the T line is not copied,
  but the package clips at row 0 and the status bar repaints anyway;
  `WIREFRAME` reaches it once per grid row, `LINE3D` at its end.
- `pgRefreshCanvasView` region-2 band clear (95-113): rows 171 to 239
  (height 69) exactly, before `showSoftmenuCurrentPart`; the error band logic
  clears on both edges (K7 split).
- `fnGdisp` with the empty clip (`x0 = 1, x1 = 0, r0 = r1 = 20`): returns at
  the row test or at `col > x1` before `lcd_fill_rect`.

### 6.7 The G2 in-family wave sites

- `pgGlyphBoundary`/`pgStringCut` (666-707): the cap `n = TMP_STR_LENGTH − 2`
  leaves room for the NUL; the boundary walk runs before the width trim and
  checks `i < n` before reading `s[i]`, so a two-byte step past the NUL never
  reads it; a lone lead byte at the end is cut in both the capped and
  uncapped cases; a second byte with bit 7 set is kept; the trim loop removes
  whole glyphs. D17 returns L−3 at the tip and L−2 under the cap-only
  mutation; D17b's canary traced (the table row is G34R1-16); D17c/D17d fire.
- `fnGarc` same-direction arm (648-657): `ax * r` fits int64; `cross == 0`
  and `dot > 0` uses the exact span, |a2 − a1| ≥ 180° per §4.5; `cross == 0`
  and `dot < 0` (exactly 180°) draws the half circle through the non-wide
  arm; the negative-span discontinuity (a2 = −359.9995 draws a full circle)
  follows from §4.5's own |d| rule.
- `fnPview` alpha prologue and `pgCloseView` epilogue (53-57, 124-130):
  cursor hidden and `FLAG_ALPHA` cleared on entry only when `calcMode` was
  `CM_AIM`; both return on EXIT after the refresh; K8 covers the round trip.
- `fnGtextout`/`fnGdisp` type check after the position check (a non-string
  in Z at an off-clip point raises nothing): G2 code, out of scope, noted.

### 6.8 Pins and drivers cleared

- W1: (3,100) and (0,50) clear before the checks (`fnErase` at 2114);
  half-away versus toward-zero differ on 2.5 and −0.5 as claimed. W2: the
  scale-398 mutation gives 199.5 then 199 under `DEC_ROUND_DOWN`. W4:
  (399,120) asserted clear by W3; the clamp mutation fires. W5: identity
  window, swap mutation fires (the identity window cannot tell "through the
  window" from "raw pixel" for complex parts, and the row does not claim it).
  W6: `fnErase` clears (200,120) before the line.
- D8 rewrite, D13-D16, D18-D20, K7-K10, V9: each preceded by a clear or an
  explicit clear-row assertion; each named mutation traced and fires;
  `pgTestClosedView` checks flags and pixel before and after
  `refreshScreen(0)`.
- P1, P2, P3 (`c47MemInBlocks` is the allocated count, `memory.c:83`, so
  before+512 is right; `EYEPT` outside the view allocates nothing after
  `pgCloseView` zeroed the region), P5 (compile-time NaN, clamp mutation
  fires on 7 and −7), P9/P26 (`gridValid` 1 from the PLNE run;
  `processKeyAction` is one real press with the release marked processed),
  P12, P16 (skip-`fnUndo` leaves X = 0.0 real, `pgReadCoord` gives 0 ≠ 1),
  P19, P20, P23 (the origin-drawn line lands on screen under the mutation),
  P27, P28, P20b, P29 (the NULL guard in its conjunction is a deref guard,
  not a dead check): each reachable, independent, and red under its named
  mutation by hand trace.
- `pgTestSnapCanvas`/`pgTestCanvasDiff`: index `r * 50 + i` stays below
  12,000 for `r <= 239`.
- `testSuiteList.txt` placement after `integrate_cov`: consistent with the
  suite's ordering note and DESIGN-HISTORY; the tail is where siblings
  append. Not a defect in itself (§5 item 11).

### 6.9 Upstream discipline

- Churn scan over the twelve patches: zero `WS-ONLY`, zero `COMMENT-ONLY`;
  the only NEAR pairs are real one-token edits.
- `keyboard.c:1604` byte-identical to undo-history's; pretty-print-extra and
  forth-core leave the upstream line, so the three-way composition holds
  (§9.6.3 item 1). `keyboard.c:1691-1700` byte-identical to
  pretty-print-extra's including the five comment lines (§7 item 5);
  forth-core's rewritten condition makes it dead but harmless in the
  combined build, as §7 item 5 says. `keyboard.c:690/832/940` `calcMode <
  19` clauses identical to both siblings (§7 item 2). The guard arm sits
  before the SNAP arm as §9.6.2 (b) specifies; pretty-print-extra's guard is
  after it. `fnKeyUp`/`fnKeyDown` cases are additions before `default`.
- `pgEffectiveCalcMode()` one-line swaps in `fnKeyEnter`/`fnKeyCC`/
  `fnKeyDotD` and `addons.c fnTo_ms`: a switch subject cannot be replaced by
  a call; ruled §7 item 7 (G1-era).
- `defines.h` `X_SHIFT`/`Y_SHIFT` edits: modified upstream lines, ruled
  DECISION in §9.6.4 with the comment on the line; other users see unchanged
  values outside mode 21. `CM_GRAPHICS_CANVAS 21`: pure insert in the
  registry block.
- `config.c:1716` hook: pure insert after the pool rebuild, 16/15 lines from
  the sibling hooks.
- `softmenus.c:337` `ITM_NULL → -MNU_CANVAS`: the only expressible shape for
  a fixed-size row. `menu_CANVAS` (352-356): pure insert after `menu_STAT`,
  23 items all defined with the rows of §6. Row 180b mid-table insert
  despite upstream's "add at the end" note: ruled in §6 (pretty-print-extra
  owns the tail row 185b); dynamic menus are rows 0-19, no hard-coded
  `softmenu[N]` index exists in `src/c47`, `saveRestoreCalcState.c` does not
  persist `softmenuId`.
- `items.c:1097-1119` catalog stubs: pure insert in the sanctioned
  `GENERATE_CATALOGS` block. Rows 2448-2462 and 2864-2872: `CAT_FREE` rows
  filled in place; `WIREFRAME`'s `TM_LBLONLY | 99 | PTP_LABEL` matches
  upstream `PGMDRV` except `SLS/US_UNCHANGED`, which §8 item 2 requires of
  every command; `generateCatalogs.c:42` iterates `1..LAST_ITEM−1`, so the
  duplicate sentinel row (G34R1-1) does not reach the catalog. `items.h`
  inserts: the upstream `ITM_2448..`/`ITM_2864..` spare names stay; two names
  for one number is the shape forth-core uses.
- `screen.h` prototypes: pure inserts next to the PIXEL family. `screen.c`
  arms: pure inserts with comments; the `hideFunctionName` arm reproduces
  upstream's own tail.
- `testSuite.c` drivers: pure inserts far from the sibling anchors;
  `funcTestNoParam` rows follow the upstream shape.
- The manifest base `af7ad934a` differs from the siblings' bases: expected
  under the package manager; the combined gate composes the stacks.

---

## 7. Verdict

Not at this tip. The simulator does not start (G34R1-1), and the gate cannot
see it because the check lives in `c47-gtk.c` alone. The moment that row is
restored, the next user who closes the simulator with the view open gets a
bug screen at every launch until `backup.bin` is deleted (G34R1-2). Both are
one-line repairs, and both are in the file set the package touched or should
have touched; neither is in the projection, the encoding, or the window
arithmetic, which hold.

On the device the first break is the pool: a program that runs `PVIEW` and
then a plot step, followed by `EXIT` and any 3D command from the CANVAS menu,
holds 2 KB with no owner (G34R1-3). The first thing a user sees is
G34R1-4: zoom in, `ERASE`, draw again, and the surface comes back stepped
and the picture may start rotated. The silent wrong stack of G34R1-5 needs
a nearly full pool and is inherited from PLTf; upstream's own guard exists
and the owner decides whether to adopt it or document the hole.

The test contract is the weakest part of the stage, and it is what would
have caught the rest. DESIGN.md §9.8 describes 28 pins; 12 do not exist,
among them every pin that would drive the real key path, the abort, the
nesting refusal, the pointer restore and the RAM_FULL arms (G34R1-11). The
key-driven redraw has no oracle with content (G34R1-12). Four named
mutations — P14's own, P8's own, a blank redraw, a moved grid vertex — leave
the gate green. The 3D driver leaks 1,024 pool blocks per run and shifts the
layout under every later suite file (G34R1-10), which is the one
package-side fact this round can hand to the AddressSanitizer reproduction
of the open suite question; it is not the mechanism. The mechanism is
upstream's, settled after the pass in §4.1: `parseEquation` reads past the
terminator of a short formula, and the package only moves the bytes it
reads.

The five folded fixes hold for the cases their readers ran. Two are
incomplete on the inverse case the same readers named in the same sentence
(G34R1-6, G34R1-7). That pattern — pin the case the reader ran, not the
sentence the reader wrote — is the design observation that ages best from
this round.

---

## 8. Round and exit state

- **Round:** 1 of the program-graphics G3/G4 audit, subject
  `0663b2360..840fe1c92`, tip `840fe1c92` (`pkg: program-graphics README —
  the commands, the coordinates, the 3D view, and two examples`). The
  audited code is G3 (`18e222e97`), the G2 in-family fix wave (`473ed9c4a`),
  G4 with the G4 out-of-family fix wave folded in (`e19769236`), and the
  README. This report is the in-family leg. The out-of-family leg ran over
  the G3 and G4 commits before the G4 wave; its fixes are on the tip.
- **Readers, in-family:** eight finders (contracts, lifecycle, arithmetic,
  errorpaths, guards, tests, design, upstream), then three-lens verifiers
  (reachability, correctness, intent) on every finding, each instructed to
  refute. 25 verifier-confirmed entries, 7 refuted, 7 beyond the cap.
- **Readers, out-of-family:**

  | reader | packet | reply | `MODEL:` | raised | survived refutation |
  |---|---|---|---|---|---|
  | sol / gpt | `design-docs/program-graphics/audit/PACKET_G3_E_window_sol.md` | `design-docs/program-graphics/audit/REPLY_G3_E_sol.md` | `MODEL: GPT-5` | 1 (E-1) | 0. E-1 refuted: the documented limit its own round produced (§5.2, §10 item 11) |
  | gemini | `design-docs/program-graphics/audit/PACKET_G3_F_contracts_gemini.md` | `design-docs/program-graphics/audit/REPLY_G3_F_gemini.md` | `MODEL: Gemini 3.1 Pro (High)` | 2 (F-1, F-2) | 0. F-1 refuted by contract (fact 1, pin D16). F-2 beyond the cap; a documented limit (fact 2, §10 item 10) |
  | sol / gpt | `design-docs/program-graphics/audit/PACKET_G4_G_projection_sol.md` | `design-docs/program-graphics/audit/REPLY_G4_G_sol.md` | `MODEL: GPT-5` | 3 (G-1, G-2, G-3) | 0 open. G-2 and G-3 were fixed on the reader's word (P27, P28); the fixes hold and are complete (§4.2, eight in-family clearances, no three-lens verdict). G-1 beyond the cap; no writer (fact 3) |
  | gemini | `design-docs/program-graphics/audit/PACKET_G4_H_engine_gemini.md` (56,847 B, above the proven packet size) | `design-docs/program-graphics/audit/REPLY_G4_H_gemini.md` | `MODEL: Gemini 3.1 Pro (High)` | 2 (H-1, H-2) | 0 open as filed; 1 follow-on. H-1 fixed by removal, complete for the armed case; the disarmed case is G34R1-5. H-2 fixed (P29), complete on the flag; the write path behind it is G34R1-7 |
  | sol / gpt | `design-docs/program-graphics/audit/PACKET_G4_I_commands_sol.md` | `design-docs/program-graphics/audit/REPLY_G4_I_sol.md` | `MODEL: GPT-5.6` | 1 (I-1) | 0 open as filed; 1 follow-on. I-1 fixed (P20b) for the wide span; the narrow span Sol named in the same sentence is G34R1-6 |

- **Tally:** 25 verifier-confirmed entries → 18 distinct findings in §3
  (1 crash-class in the simulator, 1 stuck-state, 2 wrong-result, 7 latent,
  7 design-flaw), one of them (G34R1-5) carrying a recorded split between
  the reachability and intent lenses. 7 refuted entries → §6.2. 7 claims
  beyond the cap → §4.2, none open. The open suite question stays open
  (§4.1). All three families read the actual subject, so the round-1
  three-family rule is met and the round counter advances to 1.
- **Exit criterion:** not met, and by ruling not applied. CODE_AUDIT.md
  asks for two consecutive clean rounds with one out-of-family, and a real
  finding resets the count. PLAN §10, Stan's ruling of 2026-09-04 evening:
  "one round of bug hunting and fix is enough, then move on." Each stage
  gets one audit round (in-family plus the outside readers), one fix wave
  with red-first pins, then the next stage. The out-of-family half of this
  round and its fix wave are on the tip. This report is the in-family half;
  its fix wave is not written. Under the ruling, G3 and G4 advance after one
  wave for §3, with the rulings G34R1-4, G34R1-5, G34R1-8 and G34R1-9 need,
  and no second round follows unless Stan asks.
- **What the round did not close:** the open suite question stayed open
  through the pass and was settled by the operator afterwards (§4.1): an
  upstream over-read in `parseEquation`, not a package defect; the upstream
  report is not written. The
  seven claims of §4.2 have no three-lens verdict; the five fixes among them
  were cleared by every in-family finder independently. G34R1-2 was read,
  not executed, because G34R1-1 stops the simulator before `restoreCalc`.
  Compiler warnings were not collected. Packet H's clearances outside the
  **[H]** marks carry the fact-7 caveat.
- **The fix wave is unaudited code, again.** Whatever wave closes §3 is the
  last unaudited code of these stages under the ruling. The class tests in
  §3 stand in for the second look; G34R1-6 and G34R1-7 show what happens
  when a wave pins the case and not the sentence.
- **Housekeeping:** every verifier left its worktree clean at `840fe1c92`
  (probes reverted by inverse edit, generated files re-refreshed, build
  byproducts restored or removed). This pass ran `design-audit.sh`
  read-only and wrote only this file. The main working tree is on
  `program-graphics/stage-g0` at the tip with unrelated modifications that
  are not this audit's. Filename cut after the tip per the G2-report
  convention; the full subject is in §1.
