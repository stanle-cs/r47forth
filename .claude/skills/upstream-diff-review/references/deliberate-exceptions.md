# Deliberately non-minimal patch shapes — NOT findings

Each entry is a ruled or designed decision. A minimality review that flags
one of these is wrong; a fix that "cleans one up" undoes a paid-for
decision. Check here before flagging any modified or deleted upstream
line. (Additions to this file need the ruling's citation — an entry
without one is a claim, and claims get checked.)

| shape | where | why it stands | citation |
|---|---|---|---|
| Wholesale deletion of `_executeOp` / `_executeWithIndirectRegister` / `_executeWithIndirectVariable` (−237 lines) | `010-programming__lblGtoXeq.c.patch` | The H2 hook lives inside the extracted `param_core.c` (Forth resolution order must be woven into parameter dispatch); the superseded upstream copy is deleted **so upstream edits there conflict loudly at integrate time instead of landing silently in dead code**. Clash-seeking on purpose. | DESIGN.md:1861 (H2, 2026-08-04) |
| 26 in-file renames `leaveTamModeIfEnabled()` → `_tamLeave()` | `010-ui__tam.c.patch` | D7-1: internal leave-then-dispatch sites must get the RAW teardown, never the fold-settling public wrapper; the wrapper keeps upstream's exact name/signature so the six un-overridden upstream caller files inherit the fix through the link. The hunk count is inherent to the semantics. Known residue: a future upstream in-file caller of the public name is the undefendable direction (C-5, documented). | DESIGN_D7-1_tamFinish_2026-08-08.md, approved rev 2 |
| `forthUserItemDispatch` replacing six `CM_PEM ? insertUserItemInProgram : reallyRunFunction` pairs | items.c ×2, keyboard.c ×2, screen.c ×2 | The dispatch became three-way (PEM records / live interactive capture inserts text / else executes) — not expressible as an addition; superseded pairs deleted per the H2 convention. | items.h hook comment (L1-3); round-6 F8 |
| `runFunction` XEQ dynamic-menu arm rewritten around `forthResolveXEQ` | `010-items.c.patch` | Forth resolution order (DESIGN.md §4) must sit inside the resolution; the arm's comment carries the b8f79e486 rebase reasoning (strict superset of upstream's GLOBAL_LABELS fix). | DESIGN.md §4.2, H3 |
| Mid-table `softmenu[]` insert at slot 022 despite upstream's "add at the end" note | `010-softmenus.c.patch` | `softmenu[]` dynamic-menu order must match `dynamicSoftmenu[]` (upstream's own comment, softmenus.c:1021-1028); MNU_FORTH must sit adjacent to the dynamic block. | P-H5 (DESIGN.md §6) |
| `NUMBER_OF_DYNAMIC_SOFTMENUS` 22 → 23 | `010-defines.h.patch` | Upstream's own documented procedure for adding a dynamic menu; override stays byte-identical except this line and the ASSIGN band define. | P-H6 (DESIGN.md §6) |
| `FORTH_SELFTEST_EXPORT` replacing `static` on `executeFunction`, `_closeCatalog`, `determineItem` (+ screen.c console statics) | keyboard.c, screen.c | Self-test builds must drive the real functions, not hand-rolled pop sequences that silently test the wrong thing; production linkage unchanged (macro expands to `static`). | keyboard.c banner (K1/C2 precedent) |
| Item table rows 213 / 2842 / 2843 replaced in place | `010-items.c.patch` | Filling spare `CAT_FREE` slots is the sanctioned mechanism (may fill, must not grow past `LAST_ITEM`); generator stubs added per README's items.c rule. | DESIGN.md §0.1-0.2, H1 |
| `param_core.c` as a diverged fork of deleted upstream code (bounded reads, `end` params, XEQ fallback) | `files/programming/param_core.c` | The F2 extraction with F2-2 hardening; divergence is the point. Its header comment claiming "byte-identical" is a known doc-drift finding (2026-08-09 review §3) — fix the comment, not the fork. | DESIGN.md §10.2, F2/F2-2 |
