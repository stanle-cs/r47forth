# Upstream report — one finding in `src/c47/c47Extensions/addons.c`

**Version:** base commit `70f8b7db7` (this package's `.refresh-manifest.json`
base). The file is **unmodified** by this package and by both sibling packages
— their two `addons.c` hunks are at `:663` and `:1427`, far from the site.

**How it was found:** an AddressSanitizer build of the simulator test suite,
2026-08-30. The project's normal simulator build sets `b_sanitize=none`, so
this had never run before. The report is a machine-verified read, not a code
inspection.

Paste as its own issue.

---

## `checkForAndChange` reads `resultingIntStr[-1]` whenever the integer part is empty

**File:** `src/c47/c47Extensions/addons.c:3249`, in `checkForAndChange`.

### The line

```c
if((resultingIntStr[stringByteLength(resultingIntStr)-1]==' ' || resultingIntStr[max(0, stringByteLength(resultingIntStr)-1)]==0) &&  denomStr[0]=='/' && cStr[0]==0) {
```

The two subscripts are the same expression. The second is clamped with
`max(0, ...)`; the first is not. `||` evaluates left to right, so the
**unclamped** read always happens first and the clamp on the second term can
never protect it.

### When it fires

`resultingIntStr` is declared at `:3054` and zeroed at `:3058`:

```c
char denomStr[20], wholePart[30], resultingIntStr[100], tmpstr[50];
...
resultingIntStr[0] = 0;
```

It is written only inside conditional branches (`:3211`, `:3215`, `:3219`,
`:3230`). A value with **no integer part** takes none of them, so the string is
still empty at `:3249`. `stringByteLength` returns 0, the subscript is `-1`,
and the read lands one byte before the array.

### Reaching input

Set `IRFRAC` (irrational-fraction display), clear `FRACT`, and display
`√2` — `1.414213562373095048801688724209698`:

```
FIX 4                  (any format)
IRFRAC on, FRACT off
1.414213562373095048801688724209698   →  displayed
```

The call chain measured is
`real34ToDisplayString` → `real34ToDisplayString2` (`display.c:538`) →
`checkForAndChange` (`addons.c:3249`).

### What ASAN says

```
ERROR: AddressSanitizer: stack-buffer-overflow
READ of size 1 at 0x... thread T0
    #0 checkForAndChange   c47Extensions/addons.c:3251   [3249 + 2 sibling-package lines]
    #1 real34ToDisplayString2  display.c:538
    #2 real34ToDisplayString   display.c:253
...
Address ... is located in stack of thread T0 at offset 1231 in frame checkForAndChange
    [1232, 1332) 'resultingIntStr' (line 3056)
        <== Memory access at offset 1231 underflows this variable
```

### Severity

A one-byte read of the adjacent stack slot. It does not corrupt anything and
the drawn text is correct, which is why it has never been noticed. It is still
undefined behaviour, and what it reads is whatever the frame layout puts
there — which is a compiler and target decision, not a source one.

### Suggested fix

Clamp the first subscript the same way the second one already is, or test the
string for empty before either:

```c
const int16_t last = (int16_t)stringByteLength(resultingIntStr) - 1;
if(last >= 0 && (resultingIntStr[last]==' ' || resultingIntStr[last]==0)
    && denomStr[0]=='/' && cStr[0]==0) {
```

Note that with an empty string the original condition's intent is ambiguous:
`resultingIntStr[max(0,-1)]` reads `resultingIntStr[0]`, which is the
terminator, so the second term is TRUE for an empty string. If that is the
intended behaviour, the guard should say so:

```c
if((last < 0 || resultingIntStr[last]==' ' || resultingIntStr[last]==0) && ...
```
