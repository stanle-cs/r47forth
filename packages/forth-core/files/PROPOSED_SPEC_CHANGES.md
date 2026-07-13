# Proposed spec changes (forth-core)

Non-authoritative. DESIGN.md is read-only for the agent; anything below is a
proposal for a human maintainer to fold into DESIGN.md (or upstream), not a
change already ratified there.

---

## Range-overlap double-free guard in `freeListFree`

**RATIFIED** — promoted to DESIGN.md §5.6 and §6 hook H10 (COMMIT 12).
Implementation lives in `packages/forth-core/core/freeList.c` (already
registered in `pkg_override_sources`). Tests in
`packages/forth-core/test_dict_reloc.c` (FIX-6 section).

**Pending:** upstream MR to c47 firmware repository.
