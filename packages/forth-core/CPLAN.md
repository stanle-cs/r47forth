Sub-Phase C — Ordered Step Plan (Corrected v2)
Step 1: C-10a — 64KB offset-wrap guard in forthDictEnsure
Sections: C-10 (partial)  
Files: forth_dict.c  
Change: Add (uint32_t)fdict.here + neededBytes > 0xFFFEu check at the very top of forthDictEnsure, before the lazy-alloc block. On hit: displayCalcErrorMessage(ERROR_RAM_FULL, ...); return false;  
Rationale: Prevents silent uint16_t wrap on 256 KB hardware. Guards an existing function, no dependencies.  
Test: In forthDictSelfTest, force fdict.here = 0xFFFC, call forthDictEnsure(4), assert returns false.  
Build: ./packages/forth-core/build-test.sh
Step 2: C-3 — FORTH_PRIM_NONE sentinel
Sections: C-3  
Files: forth_dict.h  
Change: Add #define FORTH_PRIM_NONE ((uint16_t)0xFFFFu) after line 25.  
Rationale: forthFindPrim returns (uint16_t)-1 (forth_dict.c:122). The compiler must test != FORTH_PRIM_NONE — comparing >= 0 on unsigned is always true. Header-only, zero runtime impact.  
Test: Compile check only.  
Build: ./packages/forth-core/build-test.sh
Step 3: C-7 — Export push helpers + confirm longInteger conformance
Sections: C-7  
Files: forth_inner.c, forth_dict.h  
Changes:
- forth_inner.c line 17: static void forthPushReal34 → void forthPushReal34
- forth_inner.c line 33: static void forthPushInt32 → void forthPushInt32
- forth_dict.h: Add declarations:
void forthPushReal34(const real34_t *val);
void forthPushInt32(int32_t val);
- Verify (no change expected): forthPushInt32 (forth_inner.c:33-43) already uses longIntegerInit / int32ToLongInteger / convertLongIntegerToLongIntegerRegister(li, REGISTER_X) / longIntegerFree — NOT int32ToReal34. If the body still calls int32ToReal34, rewrite it to the longInteger path. (Current committed code is correct; this step only removes static.)  
Rationale: Compiler needs these in interpret state without reimplementing the lift discipline. LongInteger path satisfies C-8 number-type conformance.  
Test: Existing tests pass unchanged (visibility only).  
Build: ./packages/forth-core/build-test.sh
Step 4: C-6 — Glyph-wise tokenizer (pointer-based, no external buffer)
Sections: C-6  
Files: forth_compile.c  
Changes: Add to forth_compile.c:
- Headers: #include "c47.h", #include "charString.h" (for stringNextGlyph), #include "sort.h" (for compareString)
- #define FORTH_TOKEN_MAX 63
- static const char *currentSource; — set by caller, NOT by referencing an external buffer
- #define forthSourceInit(src) do { currentSource = (src); pos = 0; } while(0)
- static int16_t pos;
- bool_t nextToken(char *buf) — operates on currentSource:
- Skip spaces via stringNextGlyph(currentSource, pos)
- Scan non-space glyphs via stringNextGlyph
- NUL-terminate, cap at FORTH_TOKEN_MAX, error on overflow
- Returns false at end of line
Rationale: Tokenizer is self-contained — no dependency on forthSource buffer in forth_bridge.c. Tests pass string literals directly to forthSourceInit.  
Test: New test function in forth_compile.c (or test_dict_reloc.c): forthSourceInit("DUP + 3.14"); verify tokens "DUP", "+", "3.14", then false. Edge: two-byte glyphs, spaces embedded in glyphs.  
Build: ./packages/forth-core/build-test.sh
Step 5: C-8 — Number grammar + classification
Sections: C-8  
Files: forth_compile.c  
Changes: Add to forth_compile.c:
- static bool_t containsTwoByteGlyph(const char *token):
Scan byte-by-byte: if ((uint8_t)token[i] >= 0x80) return true; — must cast to uint8_t before comparison. On signed-char architectures, token[i] >= 0x80 is a tautological false (signed char max is 0x7F), so every two-byte glyph would be misclassified as a valid number character.
- typedef enum { NUM_NONE = 0, NUM_INT, NUM_REAL } numType_t;
- static numType_t classifyNumber(const char *token) — byte-level grammar: [+-]?digit+ is int; same with . or e/E is real; anything else is NUM_NONE. Rejects tokens where containsTwoByteGlyph returns true.
- static bool_t parseInt32(const char *token, int32_t *out):
1. Skip leading +
2. longInteger_t li; longIntegerInit(li);
3. if (stringToLongInteger(skipPlus, 10, li) != 0) { longIntegerFree(li); return false; }
4. Range check: longIntegerCompareInt(li, INT32_MAX) <= 0 && longIntegerCompareInt(li, INT32_MIN) >= 0
5. In range: longIntegerToInt32(li, *out); longIntegerFree(li); return true;
6. Out of range: longIntegerFree(li); return false; — longIntegerFree MUST execute before return false here; omitting it leaks the GMP-allocated limb storage on every out-of-range integer.
7. No implicit fall-through — every exit path frees li.
- static bool_t parseReal34(const char *token, real34_t *out) — stringToReal34(token, out) (decQuadFromString).
Rationale: Grammar gate prevents decQuadFromString from accepting "NaN"/"Infinity". Integers → long integer type (via C-7 helper), reals → real34.  
Dependencies: Step 3 (push helpers available for test use).  
Test: "42" → NUM_INT, parseInt32 → 42. "3.14" → NUM_REAL. "1e5" → NUM_REAL. "NaN" → NUM_NONE. "+7" → NUM_INT. "-100" → NUM_INT. "99999999999" → NUM_INT but parseInt32 returns false → fall to real34 path.  
Build: ./packages/forth-core/build-test.sh
Step 6: C-9 + C-10b — Dict-emit API + count cap
Sections: C-9, C-10 (count cap)  
Files: forth_dict.c, forth_dict.h  
Changes:
- forth_dict.c: static struct { uint16_t here, latest, count, entryOff; bool_t open; } openDef;
- forth_dict.c: bool_t forthDictEmit(ftoken_t tok) — forthDictEnsure(2), memcpy(fdict.base + fdict.here, &tok, 2), fdict.here += 2
- forth_dict.c: bool_t forthDictEmitBytes(const void *src, uint16_t nBytes) — loop nBytes by 2, emit each cell
- forth_dict.c: bool_t startDefinition(const char *name):
- nameLen = strlen(name), validate 1..FORTH_NAME_MAX
- C-10b count cap: if (fdict.count >= 0x6F00) { ERROR_RAM_FULL; return false; }
- Snapshot {here, latest, count} into openDef
- off = forthDictAllocate((uint8_t)nameLen, 0) — already writes header with FF_SMUDGE, updates latest/count/here
- forthDictWriteName(off, name)
- Zero pad bytes: for (i = off + 4 + nameLen; i < forthDictBodyStart(off); i++) fdict.base[i] = 0
- openDef.entryOff = off; openDef.open = true;
- forth_dict.c: bool_t finishDefinition(void) — forthDictEmit(FTOK_EXIT), forthDictFinishDef(openDef.entryOff), openDef.open = false
- forth_dict.c: void abortDefinition(void) — if open: restore snapshot, openDef.open = false
- forth_dict.h: Declare all five functions
Rationale: Foundation for compile-state emission. forthDictAllocate does header writing per C-9 spec. Count cap prevents FTOK_CALL (0x7F00+) collision.  
Dependencies: Step 1 (offset wrap guard protects forthDictEnsure called by every emit).  
Test: Define word via API, emit body tokens, finish, verify body. Test abort — fdict.count restored. Test count cap at 0x6F00.  
Build: ./packages/forth-core/build-test.sh
Step 7: C-5 + C-4 + C-2 — fnForthOuter + forthOuterInterpret with COMPLETE interpret-state resolution
Sections: C-5 (outer entry), C-4 (STATE machine, error cases), C-2 (lookup order), C-7 (ASLIFT discipline), C-12 (error code)  
Files: forth_bridge.c, forth_compile.c, forth_dict.h  
Changes:
forth_bridge.c — replace stub entirely:
- static char forthSource[256]; — PRIVATE to this file, NOT exposed externally
- static bool_t forthOuterActive = false;
- fnForthOuter(uint16_t unused):
- Re-entrancy guard: if (forthOuterActive) { displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ...); return; } — C-12: must be ERROR_OPERATION_UNDEFINED, NOT ERROR_RAM_FULL
- Validate X is dtString, copy to forthSource (cap at 256, no silent truncation)
- fnDrop(NOPARAM) — consume source line FIRST
- forthOuterActive = true; forthOuterInterpret(forthSource); forthOuterActive = false;
forth_compile.c — add forthOuterInterpret:
- typedef enum { STATE_INTERPRET, STATE_COMPILE } compileState_t;
- void forthOuterInterpret(const char *source):
- forthSourceInit(source); — sets currentSource and pos = 0 (Step 4's API)
- compileState_t state = STATE_INTERPRET;
- bool_t lineOK = true; — tracks whether an error occurred
- Main loop: while (lineOK && nextToken(buf)):
- C-4 error cases (interpret state):
- : in COMPILE → ERROR_OPERATION_UNDEFINED, lineOK = false
- ; in INTERPRET → ERROR_OPERATION_UNDEFINED, lineOK = false
- : with no following word → ERROR_OPERATION_UNDEFINED, lineOK = false
- C-2 lookup order (interpret state — COMPLETE in this step):
1. Prim: idx = forthFindPrim(buf); if (idx != FORTH_PRIM_NONE) { forthPrims[idx].fn(); clearSystemFlag(FLAG_ASLIFT); if (lastErrorCode != ERROR_NONE) { lineOK = false; } continue; }
2. Colon: if (forthFindColon(buf, &widx)) { forthInner(widx, programRunStop == PGM_RUNNING); clearSystemFlag(FLAG_ASLIFT); if (lastErrorCode != ERROR_NONE) { lineOK = false; } continue; }
3. Number: if (classifyNumber(buf) != NUM_NONE) { ... push; clearSystemFlag(FLAG_ASLIFT); if (lastErrorCode != ERROR_NONE) { lineOK = false; } continue; } — int: forthPushInt32; real: forthPushReal34
4. Label: label = findNamedLabel(buf); if (label != INVALID_VARIABLE) — PGM_RUNNING wrap: saved = programRunStop; programRunStop = PGM_RUNNING; reallyRunFunction(ITM_XEQ, (uint16_t)label); if (programRunStop == PGM_RUNNING) programRunStop = saved; clear ASLIFT, error check
5. Undefined: displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ...); lineOK = false;
- After every action: clearSystemFlag(FLAG_ASLIFT), check lastErrorCode != ERROR_NONE → lineOK = false
- End of line (C-4):
- If state == STATE_COMPILE and lineOK (unterminated def): abortDefinition(); displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ...);
- ASLIFT-on-exit (FIXED): if (lineOK && state == STATE_INTERPRET) setSystemFlag(FLAG_ASLIFT); — the flag is set ONLY when the line completes successfully AND we're not mid-definition. All error paths (including compile-state abort) bypass this flag. Setting ASLIFT after an error would cause the next digit entry to lift over garbage or an error-corrupted stack.
forth_dict.h: Declare void forthOuterInterpret(const char *source);
Rationale: Interpret state is the COMPLETE execution path — resolution order, number handling, label fallback, error discipline — all exercised here. Compile state (: / ;) is a gate only: : → error, ; → error. The resolution structure is shared; Step 8 injects if (state == COMPILE) branches.  
Dependencies: Steps 2 (sentinel), 3 (push helpers), 4 (tokenizer), 5 (number grammar), 6 (emit API — needed for C-4 abort path, though not emitted yet).  
Test: "3 DUP +" → X=6, ASLIFT set. "42" → X=42 (dtLongInteger). "3.14" → X=3.14 (dtReal34). "DROP" → executes. Undefined word → error, ASLIFT NOT set. ; → error, ASLIFT NOT set. : → error. Re-entrancy → ERROR_OPERATION_UNDEFINED.  
Build: ./packages/forth-core/build-test.sh
Step 8: C-1 + C-11 — Compile-state emission branches + compilation errors
Sections: C-1 (emit restrictions), C-4 (compile errors), C-11 (immediacy)  
Files: forth_compile.c  
Changes: Inject if (state == STATE_COMPILE) branches into Step 7's existing resolution structure:
- : handling (FIXED name-grab sequence):
1. Match token "+" (string compare with :)
2. Call nextToken(buf) to fetch the target definition name
3. If nextToken returns false (EOF — no name follows :): displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ...); lineOK = false; — do NOT call startDefinition with : or with an empty buffer
4. If name obtained: if (!startDefinition(buf)) { lineOK = false; } — startDefinition already errors on RAM full; on success, set state = STATE_COMPILE
5. continue to process remaining tokens under compile state
- ; handling: if (!finishDefinition()) { abortDefinition(); lineOK = false; } — state = STATE_INTERPRET
- Prim (compile): if (state == COMPILE && !(forthPrims[idx].flags & FF_IMMEDIATE)) { if (!forthDictEmit(idx + FTOK_PRIM_BASE)) { abortDefinition(); lineOK = false; } } else { forthPrims[idx].fn(); ... } — C-11: immediate primitives execute even in compile state
- Colon (compile): if (state == COMPILE) { if (!forthDictEmit(FTOK_CALL_BASE + widx)) { abortDefinition(); lineOK = false; } } else { forthInner(widx, ...); } — C-11: colon words always compile as FTOK_CALL, no immediate check
- Number (compile):
- Integer in range: forthDictEmit(FTOK_ILIT); forthDictEmitBytes(&v, 4) (2 cells of LE int32)
- Integer out of range or real: forthDictEmit(FTOK_LIT); forthDictEmitBytes(&r, 16) (8 cells of real34)
- Any emit failure → abortDefinition(); lineOK = false;
- C47 label (compile): C-1: raise ERROR_OPERATION_UNDEFINED, abortDefinition(), lineOK = false — no FTOK_C47 emission in stage C
- Undefined word (compile): abortDefinition(); displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ...); lineOK = false;
- End of line (compile): Step 7's existing state == STATE_COMPILE && lineOK path already calls abortDefinition() + error for unterminated definitions.
Rationale: Step 7's resolution order is untouched. Step 8 wraps each arm with if (state == COMPILE) to emit instead of execute. C-1 restrictions enforced: no FTOK_C47, no FTOK_BR/0BR emission. The : name-grab sequence ensures startDefinition receives the actual word name, not the colon token.  
Dependencies: Step 6 (emit API), Step 7 (resolution structure).  
Test:
- : SQ DUP * ; then 3 SQ → X=9
- : TEST 42 ; then TEST → X=42 (dtLongInteger)
- : A 3 ; : B A DUP + ; then B → X=6
- ; in interpret → error
- : : + nested → error + abort
- : FOO DUP (no ;) → abort, FOO invisible
- : with no following word → error
- C47 label in compile → error + abort
- Number vs label collision per C-2
- Register forthCompileSelfTest in forth_dict.h (PC_BUILD block) and config.c override  
Build: ./packages/forth-core/build-test.sh
Step 9: C-13 — Doc update (DESIGN.md §5.4 cost formula)
Sections: C-13  
Files: DESIGN.md  
Change: Replace cost(word) = ceil4(4 + nameLen) + 2*(tokenCount + 1) with cost(word) = ceil4(4 + nameLen) + 2 * cells, where cells = 1(EXIT) + per-token: PRIM/CALL 1, ILIT 3, LIT 9, BR/0BR 2, C47 2 or 3.  
Rationale: Documentation only.  
Build: ./packages/forth-core/build-test.sh (passes unchanged)
Corrected Dependency Graph
Step 1 (C-10a offset wrap) ─────────────────────────────┐
Step 2 (C-3 sentinel) ──────────────────────────────────┤
Step 3 (C-7 export) ────────┬───────────────────────────┤
                            │                           │
Step 4 (C-6 tokenizer) ─────┤                           │
                            │                           │
Step 5 (C-8 numbers) ───────┤─────────────────┐         │
                            │                 │         │
Step 6 (C-9+C-10b emit) ────┤─────────────────┤         │
                            │                 │         │
Step 7 (C-5+C-4+C-2 interp) ←─────────────────┤         │
                            │                 │         │
Step 8 (C-1+C-11 compile) ──┼─────────────────┘         │
                            │                           │
Step 9 (C-13 doc) ──────────┴───────────────────────────┘
Ambiguities / Missing Dependencies (Updated)
1. forthSource isolation (FIXED): Step 4's tokenizer uses static const char *currentSource set by forthSourceInit(). Step 7's fnForthOuter owns static char forthSource[256] privately in forth_bridge.c. forthOuterInterpret bridges them: calls forthSourceInit(forthSource) at entry. No cross-file linkage.
2. longIntegerToInt32 calling convention: longIntegerType.h:26 — macro do { int = mpz_get_si(op); } while(0), takes TWO args: longIntegerToInt32(li, v). Step 5 must use this form.
3. forthCompileSelfTest registration: Must be added to forth_dict.h PC_BUILD block AND config.c override. Both are package files.
4. xcopy availability: charString.h:125 declares xcopy. forth_bridge.c needs #include "charString.h" or it may come transitively through c47.h. Verify at build.
5. findNamedLabel header: programming/manage.h:30 — forth_compile.c needs this include for the label fallback in Step 7.