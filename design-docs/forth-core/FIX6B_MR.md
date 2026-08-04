# FIX-6B upstream MR: branch prepared, push is yours

Prepared 2026-08-03. The branch exists in this repo; no GitLab credentials
live on this machine, so the fork/push/MR steps are yours. Everything
below them is ready to paste.

## What is on the branch

Branch `fix/freelist-halt-on-overlapping-free`, one commit (`92bacdeb5`),
based on `26ec91634` (upstream master today, so it applies clean). It
inserts the 15-line overlap guard into pristine
`src/c47/core/freeList.c`: scan at the top of `freeListFree`, on a hit
`displayBugScreen(...)` and return, list untouched. The code lines are
byte-identical to the FIX-6B hunk our gate builds green
(`packages/forth-core/core/freeList.c`); only the comment was rewritten
so it reads standalone in upstream's tree, without fork-internal labels.

Commit author is `Stan <trungdle.work@gmail.com>`. If you want your full
name on it:

```bash
git commit --amend --author="Your Name <trungdle.work@gmail.com>" --no-edit
```

The commit ends with a Claude co-author trailer; drop it while amending
if you'd rather not carry it upstream.

## Push and open

Fork `https://gitlab.com/rpncalculators/c43` in the web UI if you have
not already, then:

```bash
git remote add gitlab-fork https://gitlab.com/<your-gitlab-user>/c43.git
```

```bash
git push gitlab-fork fix/freelist-halt-on-overlapping-free
```

GitLab prints a create-merge-request URL on push; follow it, target
`rpncalculators/c43` branch `master`.

## MR title

```
fix(freeList): halt on an overlapping or double free
```

## MR description (paste as-is)

```
This is the fail-loud version from the freeListFree discussion.

Today an overlapping or double-freed range goes into the free list. The
"Memory freeing A/B" diagnostics are compiled out on DMCP builds, and on
PC they print and fall through. Either way the list is mutated, and a
later allocation can hand the same blocks out twice.

The patch scans the free list at the top of freeListFree. On an overlap
it raises the firmware bug screen and returns with the list untouched.
There is no #if around it; device and PC run the same 15 lines. I
dropped the backtrace block from the earlier version, since the
simulator's console messages already cover triage on PC.

One question before merging. freeListFree runs inside allocation and
restore paths, like the DELall restore that frees the stats block. If an
outer operation resets calcMode afterwards, an immediate raise gets
swallowed. Is that a real concern anywhere in the call graph? If it is,
I'll switch this to a latched raise: set a fault flag here, surface it
at the next refresh. You know the callers better, so it's your call.

The detection scan has run in our fork for a few weeks with no
regressions. The bug-screen response is the rework agreed in the
discussion.
```

## After it is open

Move the runbook §3 FIX-6B row to done with the MR link, and note
upstream's answer on immediate versus latched raise in
`UPSTREAM_REPORTS_b8f79e486.md` §3 when it comes.
