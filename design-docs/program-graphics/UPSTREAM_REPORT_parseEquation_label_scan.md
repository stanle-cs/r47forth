# Upstream report: one finding in `src/c47/solver/equation.c`

Note to the person who posts this report: paste the text below the line
as one issue. Delete this note first.

---

## `parseEquation` reads bytes after the end of a short formula when it searches for a label

**Version:** upstream commit `057b62fc0` of 2026-08-03. `VERSION1` at that
commit is `00.109.04.00b0`. The code of the scan is the same on `master`
at `da8d3fe2a` of 2026-09-04.

**File:** `src/c47/solver/equation.c`, function `parseEquation`. The label
scan runs before the main parse loop. It is at lines 1307 to 1318 at
`057b62fc0` and at lines 1420 to 1431 on `master`.

**How we found it:** We found the defect on 2026-09-05 with the test suite
in `src/testSuite`. Our build adds code that is not in upstream. That code
changed only the order of the allocated blocks in the memory pool. The
defect is in upstream code.

### Terms

- The memory pool is the block memory that `allocC47Blocks` and
  `freeC47Blocks` manage. One block is four bytes (`BYTES_PER_BLOCK`).
- `setEquation` stores a formula in `TO_BLOCKS(length + 1)` blocks. Here
  `length` is the byte count of the formula without the terminator. The
  bytes between the terminator and the end of the last block are the
  padding. The block after the last block is the next block.
- A glyph is one character of the formula. It is one byte, or two bytes
  when bit 7 of the first byte is 1.
- The terminator is a byte with the value `0x00`.

### The mechanism

The scan moves a pointer forward through the formula, one glyph at a
time, for up to seven glyphs. Each time the pointer moves, the scan
compares the byte with `':'` and with `'('`. It does not compare the byte
with the terminator. If the scan ends with `labeled` false, the code
after the loop resets the pointer to the first byte.

```c
  for(uint32_t i = 0; i < 7; ++i) {
    strPtr += ((*strPtr) & 0x80) ? 2 : 1;
    if(*strPtr == ':') {
      labeled = true;
      ++strPtr;
      break;
    }
    else if(*strPtr == '(') {
      labeled = false;
      break;
    }
  }
  if(!labeled) {
    strPtr = (char *)TO_PCMEMPTR(allFormulae[equationId].pointerToFormulaData);
  }
```

A formula shorter than seven glyphs ends before the scan does. The scan
then reads the padding, and it can read the next block. The parser can
then start at the wrong byte. The scan does not change the formula.

The result depends on the padding bytes and on the bytes of the next
block:

- A `':'` at a position that the scan reads sets `labeled` to true. The
  parser then reads the bytes after that `':'` as the formula. In the case
  we saw, the result was `ERROR 45`, "Syntax error in this equation". If
  those bytes are a valid formula, the parser gives a wrong result and no
  message.
- A `'('` at a position that the scan reads stops the scan with `labeled`
  false. The reset after the loop then puts the pointer on the first byte
  of the formula, and the parser reads the formula correctly.
- With other bytes, the scan ends after seven moves with `labeled` false,
  and the parser reads the formula correctly.

A byte with bit 7 set to 1 makes the scan move two bytes. The scan then
does not read the byte after it.

The user cannot control the padding bytes or the bytes of the next block.
The user cannot predict the failure.

### Which formulas fail

The allocation size gives the rule for one-byte glyphs:

- Zero to three glyphs: the padding is zero to two bytes. The scan reads
  the padding and then the next block. The formula fails when the first
  `':'` or `'('` that the scan finds after the terminator is a `':'`.
- Four to six glyphs: the scan reads the padding. The formula fails when a
  padding byte is a `':'` from earlier use. A padding byte with bit 7
  set to 1 can also move the scan into the next block. The formula then
  fails when the scan finds a `':'` there.
- Seven glyphs or more: the scan remains inside the formula.

### What we saw

The formula was `"X"`, one glyph, in one pool block of four bytes. A
memory dump, taken when `parseEquation` ran, showed these bytes from the
start of the formula:

```
58 00 32 2d 70 3a f4 b6 1a b7 0e dc 3a b2 a1 a1
```

Byte 0 is `'X'`. Byte 1 is the terminator. Bytes 2 and 3 are the padding.
Byte 4 is the first byte of the next block. The scan moved to byte 5,
found the `':'` byte `0x3a`, and set `labeled`. The parser then started at
byte 6 and reported a syntax error.

The code alone shows that the scan reads the bytes after the terminator. Three
checks of ours also showed that the pool was not damaged:

1. We checked the free-region list (`freeMemoryRegions`) and the
   allocated-region list (`allocatedMemoryRegions`) after every test and
   at every call of `resizeProgramMemory`. The two lists together include
   every block of the pool. They have no gap and no overlap. The code did
   not free a block twice.
2. We set a hardware watchpoint on the four bytes of the formula block
   when `setEquation` wrote the formula. No write to the block occurred
   before `parseEquation` ran.
3. We added a check to `parseEquation`. When the function ran, the check
   found the formula block in the allocated-region list, with the size
   `allFormulae[equationId].sizeInBlocks`.

### How to see it

In our build, `src/testSuite/tests/integrate_cov.txt` line 13 failed when
a test of ours ran before it. Line 13 integrates the formula `X`. Our
test is not in upstream. It changes the order of the blocks in the pool.
The upstream tests `eq_cov` to `integrate_cov` in `testSuiteList.txt`
store a formula. They pass when our test runs after them.

To see the defect in the upstream firmware, put a `':'` in the block
after the formula block. Use the debugger:

1. Store the formula `X`.
2. Set a breakpoint at the scan, line 1420 on `master`.
3. Run a function that calls `parseEquation` on the formula, for example
   the integrator.
4. At the breakpoint, write `0x00` into the two padding bytes after the
   terminator.
5. Write `0x3a` into the four bytes of the next block.
6. Continue.

The scan then finds the `':'` at byte 4 and sets `labeled`. The parser
then starts at byte 5. It does not read `X`.

### Suggested fix

Compare the byte with the terminator in the loop condition, before the
pointer moves:

```c
  for(uint32_t i = 0; i < 7 && *strPtr != 0; ++i) {
    strPtr += ((*strPtr) & 0x80) ? 2 : 1;
    if(*strPtr == ':') {
      labeled = true;
      ++strPtr;
      break;
    }
    else if(*strPtr == '(') {
      labeled = false;
      break;
    }
  }
```

With this change the scan stops at the terminator of a short or empty
formula. The parser then reads the formula from its first byte. The
change has no effect on a formula with a label, because its `':'` comes
before the terminator. A labeled formula starts with a name of one to
seven glyphs and a `':'`.

Test the fix with these formulas:

- `X`, one glyph, to see the scan stop at the terminator.
- `SIN(X)`, to see the scan stop at the `'('`.
- A formula with a label, to see the scan find the `':'`.

### The same defect in `showEquation`

`showEquation` has the same scan. For a stored formula, it scans the same
string. It has no comparison with the terminator. The scan is at lines
385 to 397 at `057b62fc0` and at lines 493 to 505 on `master`.

The scan sets `inLabel` from the bytes it finds. A wrong `inLabel`
changes the display of the formula. Use the same fix with `tmpPtr` in
place of `strPtr`.
