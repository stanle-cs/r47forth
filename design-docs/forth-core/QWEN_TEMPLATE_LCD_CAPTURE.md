# QWEN_TEMPLATE_LCD_CAPTURE — LCD capture packet template (2026-08-03)

This is a TEMPLATE, not a runnable packet. The architect instantiates it
per capture job: fill every `<<...>>`, replace the fixture/shot list in
the driver, re-run the EXECUTION GATE greps against the live tree, and
paste the result to the implementer as one standalone packet. The
machinery it wraps was proven 2026-08-03 (forum/screenshots/, commit
`4022c5657`).

Binding context for any instantiation:

- Captures are packet-directed only. AGENTS.md already tells the
  implementer never to capture on its own initiative.
- The driver is TEMPORARY. It is appended, run, and then removed by the
  marker-anchored edits in step 5 — never by `git checkout`/`git
  restore` (standing §4a rule: mechanical blocks never revert the tree
  with git). A driver that survives into a commit is a defect.
- The implementer cannot see images. Its acceptance is the SHOT print
  lines plus the numeric lit-pixel floor in step 4; visual acceptance of
  the PNGs is the architect's, afterward.
- Log and todo paths for this packet are `/tmp/forth-shot-<<JOB>>-*`.
  They override any log path a previous session used.

## EXECUTION GATE (run first; any mismatch → STOP, report, do nothing)

```bash
grep -c "static int test_accept_display_parity(void);" packages/forth-core/test_dict_reloc.c
```
Expected: `1`

```bash
grep -c "fail |= test_validate_direct_corruption();" packages/forth-core/test_dict_reloc.c
```
Expected: `1`

```bash
grep -c "TEMP-LCD-CAPTURE" packages/forth-core/test_capture.part.h packages/forth-core/test_dict_reloc.c
```
Expected: `packages/forth-core/test_capture.part.h:0` and
`packages/forth-core/test_dict_reloc.c:0` (a leftover driver → STOP).

```bash
git status --short
```
Expected: empty (clean tree; a dirty tree → STOP).

## Step 1 — append the driver

Append the ENTIRE contents of
`.claude/skills/run-sim/references/capture-driver.c` to the END of
`packages/forth-core/test_capture.part.h`, then apply the packet's
fixture and shot-list replacements inside the appended block only
(the architect's instantiation lists them as exact before/after edits;
the save/restore machinery and the idiom line order are not touched).

## Step 2 — register it

In `packages/forth-core/test_dict_reloc.c`, immediately after the line
`static int test_accept_display_parity(void);` insert:

```c
static int tempLcdCapture(void); /* TEMP-LCD-CAPTURE */
```

Immediately after the line `fail |= test_validate_direct_corruption();`
insert:

```c
  printf("  [DEBUG] running tempLcdCapture...\n"); /* TEMP-LCD-CAPTURE */
  fail |= tempLcdCapture(); /* TEMP-LCD-CAPTURE */
```

## Step 3 — build and capture

```bash
./packages/forth-core/build-test.sh --build > /tmp/forth-shot-<<JOB>>-build.log 2>&1; echo EXIT=$?
```
Required: `EXIT=0`.

```bash
mkdir -p /tmp/forth-shot-<<JOB>> && cd /tmp/forth-shot-<<JOB>> && <<REPO>>/build.sim/src/c47-gtk/c47 --headless > run.log 2>&1; echo EXIT=$?; grep -a "SHOT\|PASS: LCD" run.log; ls *.bmp
```
Required: `EXIT=0`, one `SHOT n: ... dumped` line per shot in the
packet's list, the exact line `    PASS: LCD captures dumped`, and one
`.bmp` per shot. A `SHOT ... SKIP` or `SHOT FAIL` line → STOP and
report; do not retry with edits.

## Step 4 — convert and sanity-check (numeric, not visual)

```bash
cd /tmp/forth-shot-<<JOB>> && python3 -c "
from PIL import Image
import glob
for f in sorted(glob.glob('*.bmp')):
    im = Image.open(f).convert('1')
    lit = sum(1 for px in im.getdata() if px == 0)
    print(f, 'lit_pixels=', lit)
    im.convert('RGB').resize((im.width*2, im.height*2), Image.NEAREST).save(f[:-4] + '.png')
"
```
Required: every `lit_pixels` above 1000 (a near-blank frame means a
render never reached lcd_buffer — STOP and report which shot). This
floor is a capture sanity check for this temporary session only, never
a committed assertion.

## Step 5 — remove the driver (marker-anchored, no git revert)

```bash
sed -i '/BEGIN TEMP-LCD-CAPTURE/,/END TEMP-LCD-CAPTURE/d' packages/forth-core/test_capture.part.h
sed -i '/TEMP-LCD-CAPTURE/d' packages/forth-core/test_dict_reloc.c
git diff --stat packages/forth-core/test_capture.part.h packages/forth-core/test_dict_reloc.c
```
Required: the `git diff --stat` output is EMPTY (both files identical to
HEAD). Non-empty → STOP and report; do not run git checkout or restore.

## Step 6 — full gate on the restored tree

```bash
./packages/forth-core/build-test.sh > /tmp/forth-shot-<<JOB>>-gate.log 2>&1; echo EXIT=$?; grep -a -c "FORTH SELF-TEST: ALL PASSED" /tmp/forth-shot-<<JOB>>-gate.log
```
Required: `EXIT=0` and count `1`.

## Report

List: the PNG paths in `/tmp/forth-shot-<<JOB>>/`, each shot's SHOT
line verbatim, each `lit_pixels` value, and the step-5 and step-6
required outputs. The architect collects the PNGs and does the visual
acceptance; the implementer's job ends at the report.
