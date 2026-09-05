# Pretty-print-extra: design history (non-normative)

This package was created by the PP19 split (2026-08-31). Everything in
it shipped first inside `packages/pretty-print` (stages PP3-PP18 and
audit rounds r1-r9): the full pre-split amendment trail is
design-docs/pretty-print/DESIGN-HISTORY.md, and it stays there.
Rewriting history into this file breaks every audit
cross-reference. Amendments dated after 2026-08-31 that concern this
package's content go HERE.

## 2026-09-04: the key resolution arm became the registry range form

The program-graphics package needed the same arm position in
`keyboard.c` (the key resolution chain, before the final else). Two
insertions at one line do not merge. The arm now reads
`else if(calcMode >= 20 && calcMode <= 23)` with a registry comment, and
program-graphics carries the identical bytes. Behaviour for the browser
(mode 20) is unchanged. The gate of this package was re-run after the
amendment.
