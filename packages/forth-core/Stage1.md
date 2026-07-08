Dictionary Lookup Plan (revised)
Step 0 — Fix forthDict_t to include base
Files: forth_dict.h, forth_dict.c
Add uint8_t *base as the first field of forthDict_t, matching §1.2 exactly. Initialize to forthArena in the fdict initializer. Update forthDictPtr() and forthDictBodyStart() to use fdict.base instead of the bare forthArena pointer (so they'll work correctly when we later add relocatable support).
Step 1 — forth_prims.h: Define the primitive table types
File: forth_prims.h
Define forthPrim_t and forthPrimDef_t per §1.3. Declare forthPrims[] and forthPrimCount externs.
Step 2 — forth_prims.c: Empty primitive table
File: forth_prims.c
Define forthPrims[] as an empty static const array and forthPrimCount = 0. Satisfies invariant forthPrimCount ≤ 0x0FFF.
Step 3 — forth_dict.c: forthFindPrim
File: forth_dict.c
Linear scan forthPrims[0..forthPrimCount-1], comparing each .name against the argument with compareString(name, arg, CMP_BINARY) (case-sensitive, C47 glyph encoding). Returns the matching index, or (uint16_t)-1 on miss.
Step 4 — forth_dict.c: forthFindColon
File: forth_dict.c
Walk fdict.latest → link → … → FORTH_NULL. For each entry, read nameLen bytes from the name area, NUL-terminate into a scratch buffer, compare with compareString(CMP_BINARY). Newest-first.
Signature: bool forthFindColon(const char *name, uint16_t *widx_out). Returns true on hit (writes 0-based dictionary index to *widx_out), false on miss (out-param untouched).
Index computation: The entry found is the Nth in the chain (N=0 for fdict.latest). Its dictionary index = fdict.count - 1 - N.
Step 5 — forth_dict.c: forthDictWriteName
File: forth_dict.c
Given an entry offset and a NUL-terminated string, copy the string bytes into the name area at offset + sizeof(forthHeader_t). Needed by the compiler to populate a newly allocated header's name.
Step 6 — forth_dict.c: forthDictFinishDef
File: forth_dict.c
Clear FF_SMUDGE on the header at the given offset (allocated with FF_SMUDGE by forthDictAllocate). Bumps fdict.here to the next 4-byte boundary if not already aligned.
Step 7 — forth_dict.h: Declare all new symbols
File: forth_dict.h
Add declarations for forthFindPrim (returns uint16_t), forthFindColon (returns bool, takes uint16_t * out-param), forthDictWriteName, forthDictFinishDef. Add #include <stdbool.h> for bool.
Step 8 — Build verification
make sim CUSTOM_PKG=packages/forth-core. Expect clean compile, no linker errors. Verify new symbols with nm.
Dependencies
- Step 0 is independent (fixes existing code)
- Steps 1→2 are independent of each other and 0
- Step 3 depends on 1 (needs forthPrims declaration)
- Step 4 depends on 0 (needs fdict.base)
- Steps 5, 6 depend on nothing
- Step 7 depends on all above for correct declarations
- Step 8 validates everything
Not yet in scope
- Reverse-direction lookup (§4.2: H2/H3 overrides) — comes after forward lookup works
- Outer interpreter / compiler (§3.3) — needs tokenization, number parsing, state management
- Inner interpreter (§3.2) — needs non-empty primitive table