---
name: ste-comment-pass
description: Procedure for cleaning and rewriting package code comments to the ASD-STE100 contracts-only standard. Use when asked to do a comment pass, clean up comments, apply STE to comments, or trim comments in a package. Enforces the contract-only rule, verifies zero code change via comment stripping, and passes the package gate.
---

# ASD-STE100 Contracts-Only Comment Pass

Use this procedure to clean and rewrite code comments in a package. The procedure removes narrative noise and enforces ASD-STE100 Simplified Technical English.

## The Authority and Bar

A comment survives only if it provides:
1. **What the code does** when the function or variable name is not obvious.
2. **A caller obligation** (ordering, buffer size, sentinel, must-call functions, preconditions).
3. **A data format, layout, or invariant.**
4. **An optional short example** (input/output or byte layout) when it makes the contract easier to understand.

Delete everything else:
- Narrative explanations of C statements.
- Design rationale and architectural debates (these belong in `DESIGN.md`).
- Audit round tags (`R9-1`, `F10`, `C17`).
- Fragile `file:line` citations (these rot upon upstream rebases).

Refer to:
- [`comment-bar-contract-only.md`](file:///home/stan/.claude/projects/-home-stan-c43/memory/comment-bar-contract-only.md)
- [`code-comment-budget-rule.md`](file:///home/stan/.claude/projects/-home-stan-c43/memory/code-comment-budget-rule.md)
- [`ste-all-writing-standing-order.md`](file:///home/stan/.claude/projects/-home-stan-c43/memory/ste-all-writing-standing-order.md)

## ASD-STE100 Rules for Comments

Apply pragmatic mode:
- Limit instruction sentences to 20 words.
- Limit description sentences to 25 words.
- State conditions before commands (*"If the buffer is null, return false"*).
- Use active voice and simple tenses.
- Use `must` for obligations. Do not use ambiguous words like `should` or `might`.
- Keep examples short (1 or 2 lines) and factual.

## The Upstream Override Rule

In files that override upstream C47 files:
- Edit **only** package-added comments (the `+` lines in the diff).
- Preserve multi-package merge notes and claims registry tags.
- **Never touch upstream code or upstream comments.**

## Step-by-Step Execution Workflow

### Step 1: Establish Baseline Gate
Before editing, verify that the package builds and passes tests:
```bash
./packages/<pkg>/build-test.sh
```

### Step 2: Rewrite Comments in Package-Owned Files
Process package-owned headers and source files in `packages/<pkg>/`:
- Public and internal headers (`.h`).
- Implementation files (`.c`).
- Test drivers and test suites.

Apply the contracts-only filter and ASD-STE100 style.

### Step 3: Rewrite Package Comments in Override Files
Process working files that override upstream paths (e.g., `items.c`, `screen.c`, `keyboard.c`):
- Inspect existing package additions (`git diff` or patch files).
- Rewrite only package-added comments.
- Do not alter upstream context lines.

### Step 4: Verify Zero Code Logic Change
Run the comment-strip verification tool against the base git commit:
```bash
python3 .claude/skills/ste-comment-pass/references/comment_strip_diff.py --git HEAD packages/<pkg>/<file>
```
The script strips comments and compares normalized code tokens. It must report:
`PASS: ... is byte-identical in code logic ...`

### Step 5: Regenerate Package Patches
Run the package patch refresh tool:
```bash
python3 tools/pkg_patch_refresh.py packages/<pkg>
```

### Step 6: Scan for Patch Churn
Verify that no upstream context was inadvertently shifted or altered:
```bash
python3 .claude/skills/upstream-diff-review/references/patch_churn_scan.py packages/<pkg>/patches/*.patch
```
The scan must report 0 mechanical churn violations.

### Step 7: Run the Build and Test Gate
Execute the package build and test script:
```bash
./packages/<pkg>/build-test.sh
```
Ensure all tests compile and pass green.
