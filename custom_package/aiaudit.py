#!/usr/bin/env python3
"""Adversarial AI-tell detector.

Pattern set drawn from Wikipedia:Signs_of_AI_writing (the catalogue actual
editors use to strip LLM prose) plus the specific tells the owner has flagged
by hand in this project. Prose only: BBCode [code] and markdown fences are
stripped before analysis, since code is not the thing under audit.
"""
import re, sys, statistics

LEXICAL = {
 'significance-inflation': r'\b(crucial|pivotal|vital|underscor\w+|testament|indelible|enduring|tapestry|interplay|intricate\w*|meticulous\w*|delve|garner\w*|bolster\w*|landscape of)\b',
 'promotional': r'\b(vibrant|profound|nestled|groundbreaking|renowned|diverse array|boasts?|rich(ly)? (set|history|feature))\b',
 'copula-avoidance': r'\b(serves as|stands as|marks the|functions as|operates as|represents the|acts as)\b',
 'era-vocab': r'\b(additionally|moreover|furthermore|align(s|ed)? with|enhanc\w+|foster\w+|showcas\w+|holistic|robust|seamless\w*|leverag\w+|comprehensive|utiliz\w+)\b',
 'hedge-stack': r'\b(it(\'s| is) worth noting|it should be noted|generally speaking|in essence|essentially,|ultimately,|in summary|overall,)\b',
 'reader-address': r"\b(let(\'s| us) (explore|look|dive)|we(\'ll| will) (see|explore)|as we|you may be wondering)\b",
}

CONSTRUCTION = {
 'neg-parallel not-just':   r"\bnot (just|only)\b[^.;]{0,60}\b(but|it\'s|its)\b",
 'neg-parallel not-X-Y':    r"\b(is|are|was|were|it\'s) not\b[^.;]{0,40},\s*(it\'s|its|but|rather)\b",
 'X-rather-than-Y':         r'\brather than\b',
 'instead-of-pivot':        r'\binstead of\b',
 'participial-ender':       r',\s+(highlighting|underscoring|emphasizing|ensuring|reflecting|symbolizing|contributing|fostering|enabling|allowing|making it|which is what|which is why)\b',
 'announcing-insight':      r'\b(the (key|important|crucial) (thing|point)|what(\'s| is) (important|notable)|the (real|actual) (problem|issue|point))\b',
 'trailing-which-is':       r',\s+which is\b',
 'the-one-thing':           r'\b(the one (thing|trap|catch)|one thing (that|to)|two things)\b',
}

FORMATTING = {
 'em/en dash':        r'[—–]',
 'curly quotes':      r'[“”‘’]',
 'bold-colon list':   r'^\s*[-*•]\s*\*\*[^*]+\*\*\s*:',
 'excessive bold':    None,   # counted separately
}

def strip_code(t):
    t = re.sub(r'\[code\].*?\[/code\]', ' ', t, flags=re.S|re.I)
    t = re.sub(r'```.*?```', ' ', t, flags=re.S)
    t = re.sub(r'^(?: {4}|\t).*$', ' ', t, flags=re.M)   # indented blocks
    t = re.sub(r'\[/?\w+[^\]]*\]', ' ', t)               # remaining bbcode tags
    t = re.sub(r'`[^`]+`', ' ', t)                       # inline code
    return t

def sentences(t):
    t = re.sub(r'\s+', ' ', t)
    parts = re.split(r'(?<=[.!?])\s+(?=[A-Z(])', t)
    return [p for p in parts if len(p.split()) >= 3]

def rule_of_three(t):
    # "a, b, and c" / "a, b and c" triplets in prose
    return re.findall(r'\b\w+(?:\s+\w+){0,3},\s+\w+(?:\s+\w+){0,3},?\s+and\s+\w+(?:\s+\w+){0,3}\b', t)

def audit(path):
    raw = open(path, encoding='utf-8').read()
    prose = strip_code(raw)
    print(f"\n=== {path.split('/')[-1]} ===")
    total = 0
    for group, pats in (('LEXICAL', LEXICAL), ('CONSTRUCTION', CONSTRUCTION)):
        for name, pat in pats.items():
            hits = re.findall(pat, prose, flags=re.I)
            if hits:
                total += len(hits)
                ex = re.search(pat, prose, flags=re.I)
                ctx = prose[max(0, ex.start()-40):ex.end()+40].replace('\n', ' ').strip()
                print(f"  [{group}] {name}: {len(hits)}  e.g. ...{ctx}...")
    for name, pat in FORMATTING.items():
        if pat is None: continue
        hits = re.findall(pat, raw, flags=re.M)
        if hits:
            total += len(hits)
            print(f"  [FORMAT] {name}: {len(hits)}")
    b = len(re.findall(r'\*\*[^*]+\*\*', raw)) + len(re.findall(r'\[b\]', raw, flags=re.I))
    if b > 6:
        total += 1
        print(f"  [FORMAT] heavy bold/emphasis: {b} spans")
    r3 = rule_of_three(prose)
    if r3:
        total += len(r3)
        print(f"  [CONSTRUCTION] rule-of-three: {len(r3)}  e.g. \"{r3[0][:70]}\"")
    # stylometry: LLM prose has unusually low sentence-length variance
    s = sentences(prose)
    if len(s) >= 8:
        L = [len(x.split()) for x in s]
        cv = statistics.pstdev(L) / statistics.mean(L)
        flag = "  <-- LOW (uniform cadence)" if cv < 0.45 else ""
        print(f"  [STYLO] sentences={len(L)} mean={statistics.mean(L):.1f}w "
              f"CV={cv:.2f}{flag}")
        print(f"          shortest={min(L)}w longest={max(L)}w "
              f"under-8w={sum(1 for x in L if x < 8)}")
    print(f"  TOTAL FLAGS: {total}")
    return total

if __name__ == '__main__':
    grand = sum(audit(p) for p in sys.argv[1:])
    print(f"\nGRAND TOTAL: {grand}")
