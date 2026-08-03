# T6-1 — upstream-style Forth cases: forth_interp.txt — Part A

Origin: TESTING.md T6 (owner ruling 2026-08-03). The build glue is
landed (SIBLIST protocol; shadow c47 compat link). This packet adds the
package content: a patched case list and the pilot case file, making
Forth interpret-state cases run under UPSTREAM'S OWN runner.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` is `forth-core/stack-semantics-d1-d2`;
   `git status --short` shows ONLY modified tools/meson files (the
   landed glue awaiting this packet's commit) — do not touch them.
2. `grep -c "SIBLIST" tools/resolve_c47_src.py` → at least 2.
3. `grep -c "custom_pkg_testSuite_list" src/testSuite/meson.build` → at
   least 2.
4. `ls packages/forth-core/testSuite 2>/dev/null; echo rc=$?` — the
   directory does not exist yet (any rc is fine; just record it).

## Task — three steps

**Step 1 — materialize and patch the list.**
`python3 tools/pkg_patch_refresh.py packages/forth-core --materialize testSuite/tests/testSuiteList.txt`
Then append ONE line to the END of
`packages/forth-core/testSuite/tests/testSuiteList.txt` (last-line
ordering is deliberate — some upstream corpora are order-sensitive):

```
forth_interp
```

**Step 2 — the case file.** Create
`packages/forth-core/testSuite/tests/forth_interp.txt` with EXACTLY:

```
;*************************************************************
;**  forth-core interpret-state cases (T6 pilot)            **
;**  X carries the source line; fnForthOuter executes it.   **
;*************************************************************
In: FL_SPCRES=0 FL_CPXRES=0 SD=0 RMODE=0 IM=2compl SS=4 WS=64
Func: fnForthOuter

In:  FL_ASLIFT=1 RX=Stri:"1 2 +"
Out: EC=0 RX=LonI:"3"

In:  FL_ASLIFT=1 RX=Stri:"7 3 -"
Out: EC=0 RX=LonI:"4"

In:  FL_ASLIFT=1 RX=Stri:"2 3 * 4 +"
Out: EC=0 RX=LonI:"10"

In:  FL_ASLIFT=1 RX=Stri:"5 DUP +"
Out: EC=0 RX=LonI:"10"

In:  FL_ASLIFT=1 RX=Stri:"1 2 SWAP DROP"
Out: EC=0 RX=LonI:"2"

In:  FL_ASLIFT=1 RX=Stri:"1 2 3 4 5 6 7 8 9 10 11 + + + + + + + + + +"
Out: EC=0 RX=LonI:"66"
```

The last case forces the D3 spill (11 pushes on an 8-deep stack) under
upstream's runner — the point of the pilot.

**Step 3 — gate.**
`./packages/forth-core/build-test.sh > /tmp/forth-t6-1-gate.log 2>&1; echo "gate exit: $?"`
Success = exit 0 + both banners, AND the upstream suite step must show
the new file ran: `grep -c "forth_interp" /home/stan/c43/build.sim/meson-logs/testlog.txt`
→ at least 1, and
`grep "TESTS PASSED" /home/stan/c43/build.sim/meson-logs/testlog.txt`
shows a count of at least 12077 (the prior 12071 + these 6). Print the
gate exit, the grep results, and STOP. If any forth case FAILS inside
the upstream runner, print the surrounding 10 log lines and STOP — do
not adjust the cases; expected values are architect-ruled.
