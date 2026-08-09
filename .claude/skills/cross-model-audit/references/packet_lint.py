#!/usr/bin/env python3
"""Mechanical packet checks for the cross-model audit.

Every check below encodes a packet defect that actually happened across five
audit rounds (2026-08-06), each of which produced a confident wrong finding
that looked exactly like a good one. The linter is the same bargain as
forum/aiaudit.py: HARD hits are disqualifying, everything else is a flag to
JUDGE, not a count to drive to zero. A packet the linter passes can still be
wrong — the paraphrase class (condensing a load-bearing comment) is not
mechanically detectable, only the symptoms checked here are.

Usage: packet_lint.py <packet.md> [...]   Exit 1 on any HARD hit.
"""
import re, sys

# C names that show up capitalized without being state stamps.
CAPS_NOISE = set("""NULL TRUE FALSE EOF TODO NOTE FIXME XXX API CLI RAM ROM
LCD BMP GTK WSL ASCII UTF C47 R47 DM42 DMCP HEX BEGIN END IF ELSE THEN
UNTIL WHILE REPEAT AGAIN EXIT ENTER XEQ STO RCL PEM TAM FWRD ALPHA CAT
MNU ITM FTOK PTP NOPARAM MODEL HARD JUDGE SIZE PROBE""".split())


def fences(text):
    """(start_line, body) for every ``` fence."""
    out = []
    for m in re.finditer(r'^```[A-Za-z]*\n(.*?)^```', text, flags=re.S | re.M):
        out.append((text[:m.start()].count('\n') + 1, m.group(1)))
    return out


def comment_text(code):
    parts = re.findall(r'/\*.*?\*/', code, flags=re.S)
    parts += re.findall(r'//[^\n]*', code)
    return '\n'.join(parts)


def defined_functions(code):
    """Names that look defined here: `name(...)` heading a body."""
    defs = set()
    for m in re.finditer(r'\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{', code, flags=re.S):
        defs.add(m.group(1))
    return defs


def lint(path):
    text = open(path, encoding='utf-8').read()
    print(f"\n=== {path.split('/')[-1]} ===")
    hard = 0
    size = len(text.encode('utf-8'))

    fen = fences(text)
    code = '\n'.join(body for _, body in fen)

    # --- HARD: disqualifying, not judgment calls -----------------------------
    if not re.search(r'MODEL:', text):
        hard += 1
        print("  [HARD] no model probe: the packet never asks the reader to state "
              "its model. agy's -p-first flag order silently serves Claude; a "
              "silent fallback is indistinguishable from a good audit.")
    if not fen:
        hard += 1
        print("  [HARD] no code fence: a packet carries the code under audit "
              "inline. The reader has no repository.")
    for ln, body in fen:
        for pat, what in ((r'^\s*(\.\.\.|…)\s*$', 'bare ellipsis line'),
                          (r'/\*\s*(\.\.\.|snip|elided|omitted|etc)[^*]*\*/', 'snip comment'),
                          (r'//\s*(\.\.\.|snip|elided|omitted)\s*$', 'snip comment')):
            for m in re.finditer(pat, body, flags=re.M | re.I):
                hard += 1
                at = ln + body[:m.start()].count('\n')
                print(f"  [HARD] truncation marker ({what}) at line {at}: send WHOLE "
                      f"functions. Round 2's sed-cut packet produced a confident "
                      f"wrong finding about a copy-out tail the packet dropped.")
    # Seventh packet-defect class (round 8, caught by this linter's own fence
    # check on the SECOND packet after the first slipped through): a packet
    # assembled by concatenating extracted function bodies gets its closing
    # fence glued to the last brace — `}```  ` on one line, which Markdown
    # does not treat as a fence at all. With one code block the packet lints
    # as "no code fence"; with several, the openers pair with each other, the
    # check passes, and the reader receives the task section INSIDE a code
    # block. Cheap and exact: a fence marker must own its line.
    for m in re.finditer(r'^(.*\S)```\s*$', text, flags=re.M):
        hard += 1
        at = text[:m.start()].count('\n') + 1
        print(f"  [HARD] fence glued to content at line {at}: ```` ``` ```` must "
              f"start its own line, or Markdown does not close the block and "
              f"everything after it — including your task section — is read as "
              f"code. Extracted bodies need a trailing newline.")
    fence_lines = re.findall(r'^```', text, flags=re.M)
    if len(fence_lines) % 2:
        hard += 1
        print(f"  [HARD] odd number of fence markers ({len(fence_lines)}): a code "
              f"block is unterminated, so the prose after it reaches the reader "
              f"as code.")

    allow_imb = '<!-- lint: allow-imbalance -->' in text
    for ln, body in fen:
        net = body.count('{') - body.count('}')
        if net and not allow_imb:
            hard += 1
            print(f"  [HARD] brace imbalance ({net:+d}) in fence at line {ln}: a "
                  f"function is cut off. If the fragment is deliberate, say why "
                  f"in the packet and add <!-- lint: allow-imbalance -->.")

    # --- judged flags --------------------------------------------------------
    if code:
        named = set()
        for m in re.finditer(r'\b([A-Za-z_]\w{3,})\s*\(', comment_text(code)):
            named.add(m.group(1))
        prose = re.sub(r'```.*?```', ' ', text, flags=re.S)
        for m in re.finditer(r'`([A-Za-z_]\w{3,})\s*\(\)?`', prose):
            named.add(m.group(1))
        missing = sorted(n for n in named - defined_functions(code)
                         if not re.match(r'^(if|for|while|switch|return|sizeof)$', n))
        if missing:
            print(f"  [JUDGE] named but not defined here: {', '.join(missing)}")
            print("          If a comment names a function, that function is part of "
                  "the packet (round 5: the reader's whole finding rested on the "
                  "one that was dropped). Include each, or state in Orientation "
                  "why the reader does not need its body.")

    # Round 7's D7-a: "enumeration without a count check" was the round's
    # dominant class, and it hit the audit process itself — the round's own
    # tasking inherited an approved design's "eleven" for a population of 28,
    # and both verifiers had to re-derive the truth from scratch. A packet
    # that counts sites is making a claim, and a claim gets its command.
    prose_only = re.sub(r'```.*?```', ' ', text, flags=re.S)
    counted = re.findall(
        r'\b(both|one|two|three|four|five|six|seven|eight|nine|ten|eleven|'
        r'twelve|thirteen|fourteen|fifteen|sixteen|seventeen|eighteen|nineteen|'
        r'twenty|\d{1,3})\s+'
        r'(?:[\w-]+\s+){0,3}?'
        r'(call sites?|sites?|callers?|consumers?|arms?|writers?|places?)\b',
        prose_only, flags=re.I)
    if counted and not re.search(r'grep\s+-[a-zA-Z]*c|grep -c', text):
        shown = ', '.join(f"{n} {w}" for n, w in counted[:5])
        print(f"  [JUDGE] enumeration with no count command: {shown}")
        print("          A packet that says how many sites there are must carry "
              "the grep that produces the number, so the reader can check it "
              "instead of trusting it. The approved D7-1 design said eleven "
              "where the tree had 28, and the audit paid for it twice.")

    if not re.search(r'^##\s+Orientation', text, flags=re.M):
        print("  [JUDGE] no Orientation section: shared-structure facts, what "
              "ESTABLISHES each state, and every package override the excerpt "
              "depends on all live there. Three of five packet defects were "
              "orientation omissions.")
    if not re.search(r'\boverride', text, flags=re.I):
        print("  [JUDGE] no override mention: if any excerpt comes from or depends "
              "on a package override, the Orientation must name it (round 3: a "
              "reader with no repository cannot know the package rebuilds the "
              "FWRD picker on every paint).")
    if not re.search(r'establish', text, flags=re.I):
        caps = sorted({w for w in re.findall(r'\b[A-Z][A-Z_]{2,11}\b', text)
                       if w not in CAPS_NOISE})
        if caps:
            print(f"  [JUDGE] states without an establishing gate? caps tokens: "
                  f"{', '.join(caps[:12])}")
            print("          For every state the packet discusses, say what "
                  "ESTABLISHES it, not just what it means — 'a BORROWED frame is "
                  "always FWRD' is the claim that blocks the wrong trace.")
    if not re.search(r'packet alone|no repository|name the gap|budget', text, flags=re.I):
        print("  [JUDGE] no budget/self-containment note: Sol completes only "
              "self-contained packets and ignores every budget instruction it "
              "is not given up front.")

    kb = size / 1024
    # Round 6: four packets of 11.5-16.6 KB were all answered in minutes with
    # the model probe passing, so the proven ceiling is ~17 KB, not the ~11 KB
    # round 5 recorded. Size is a depth trade-off, not a wall; the old 13 KB
    # failure was over-read, not over-length.
    # Round 9: 22.1 KB (Gemini, whole-function fix packet) and 18.6 KB (Sol,
    # self-contained design packet) both answered well, identity passing.
    note = ('thin — is the whole function really here?' if kb < 2 else
            'proven range' if kb <= 22.5 else
            'beyond the tested range (22.1 KB Gemini / 18.6 KB Sol are the largest proven); split for depth')
    print(f"  [SIZE] {size} bytes ({kb:.1f} KB) — {note}")

    if hard:
        print(f"  {hard} HARD hit(s): fix before dispatch, no judgment call.")
    print("  Not mechanically checkable: comments condensed or paraphrased "
          "(that class is invisible here — diff them against the source).")
    return hard


if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    sys.exit(1 if sum(lint(p) for p in sys.argv[1:]) else 0)
