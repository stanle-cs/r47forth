#!/usr/bin/env python3
"""Adversarial AI-tell detector.

Pattern set drawn from Wikipedia:Signs_of_AI_writing (the catalogue actual
editors use to strip LLM prose) plus the specific tells the owner has flagged
by hand in this project. Prose only: BBCode [code] and markdown fences are
stripped before analysis, since code is not the thing under audit.
"""
import re, sys, statistics

LEXICAL = {
 'significance-inflation': r'\b(crucial|pivotal|vital|underscor\w+|testament|indelible|enduring|tapestry|interplay|intricate\w*|meticulous\w*|delve|garner\w*|bolster\w*|landscape of|evolving landscape|focal point|deeply rooted|key (role|moment|turning point)|setting the stage|reflects broader|represents a shift|marking the)\b',
 'promotional': r'\b(vibrant|profound|nestled|groundbreaking|renowned|diverse array|boasts?|rich(ly)? (set|history|feature)|in the heart of|natural beauty|commitment to|exemplif\w+)\b',
 'copula-avoidance': r'\b(serves as|stands as|marks the|functions as|operates as|represents the|acts as|refers to|features a|offers a|maintains a)\b',
 'era-vocab': r'\b(additionally|moreover|furthermore|align(s|ed)? with|enhanc\w+|foster\w+|showcas\w+|holistic|robust|seamless\w*|leverag\w+|comprehensive|utiliz\w+)\b',
 'hedge-stack': r'\b(it(\'s| is) worth noting|it should be noted|generally speaking|in essence|essentially,|ultimately,|in summary|overall,)\b',
 'reader-address': r"\b(let(\'s| us) (explore|look|dive)|we(\'ll| will) (see|explore)|as we|you may be wondering)\b",
 'superficial-analysis': r'\b(valuable insights?|resonates? with|encompass\w+|cultivat\w+)\b',
 'weasel-attribution': r'\b(industry (reports|observers)|observers have (cited|noted)|experts (argue|agree|note|suggest)|some critics (argue|say)|(several|many|numerous) (sources|publications|outlets)|widely (regarded|recognized|considered))\b',
 'challenges-formula': r'\b(faces several challenges|despite (these|its) challenges|future (outlook|prospects)|challenges and (legacy|opportunities))\b',
 'notability-canned': r'\b(independent coverage|media outlets|trade publications?|profiled in|active social media presence)\b',
}

# Disqualifying, not judgment calls: chatbot leftovers the Wikipedia catalogue
# lists as certain tells. One hit means the text passed through a model and
# nobody read it after.
HARD = {
 'chatbot-artifact':   r'(contentReference|oaicite|oai_citation|attributableIndex|turn\d+(search|view|news)\d*|\[cite:\s*\d+\]|start_span|end_span|grok_card|grok_render|ppl-ai-file-upload|attached_file|:::writing)',
 'placeholder':        r'(⟨[^⟩]*⟩|\[insert [^\]]+\]|\[[Yy]our [^\]]+\]|lorem ipsum)',
 'cutoff-disclaimer':  r"\b(as of my (last|latest) (update|training)|as an ai\b|i (cannot|can't) (browse|access)|knowledge cutoff)\b",
 'utm-tracking':       r'[?&]utm_[a-z]+=',
}

CONSTRUCTION = {
 'neg-parallel not-just':   r"\bnot (just|only)\b[^.;]{0,60}\b(but|it\'s|its)\b",
 'neg-parallel not-X-Y':    r"\b(is|are|was|were|it\'s) not\b[^.;]{0,40},\s*(it\'s|its|but|rather)\b",
 'neg-parallel appositive': r",\s+(?:not|never)\s+(?:a|an|the|on|by|from|in|at|to|of|just|only|what|where|because|somebody)\b|,\s+(?:not|never)\s+\w+[.:]",
 'neg-parallel not-justX':  r"\b, not just\b",
 'neg-parallel and-tail':   r"\band\s+(?:does|do|is|are|must|can|will)\s+(?:not|never)\s+\w+[^.;:]{0,30}[.;]",
 'X-rather-than-Y':         r'\brather than\b',
 'instead-of-pivot':        r'\binstead of\b',
 'participial-ender':       r',\s+(highlighting|underscoring|emphasizing|ensuring|reflecting|symbolizing|contributing|fostering|enabling|allowing|making it|which is what|which is why)\b',
 'announcing-insight':      r'\b(the (key|important|crucial) (thing|point)|what(\'s| is) (important|notable)|the (real|actual) (problem|issue|point))\b',
 'trailing-which-is':       r',\s+which is\b',
 'the-one-thing':           r'\b(the one (thing|trap|catch)|one thing (that|to)|two things)\b',
 # added 2026-08-25 — classes that reached the rejected undo-history r2 draft unflagged
 'split-neg-parallel':      r"\b(?:isn'?t|aren'?t|wasn'?t|is not|are not)\s+(?:just|only)\b[^.;!?]{0,60}[.!?]\s+It(?:'s| is)\b",
 'twist-tail-except':       r',\s+except\b[^,.;]{0,80}[.;]',
 'twist-tail-never':        r'\b(?:like|as if)\b[^,.;]{0,50}\bnever\s+(?:ran|happened|existed|was)\b',
 'vivid-scenario':          r"\byou\s+(?:usually|often|always|inevitably)\s+(?:notice|find|realize|end up|discover)\b|\bwe'?ve all\b|\bsound familiar\b",
 'fake-humble':             r"\bI'?m sure there\b[^.]{0,70}\bI haven'?t\b|\b(?:corners|edge cases|rough edges)\s+I haven'?t\s+(?:hit|found|covered)\b",
 'colon-elaboration':       r"\bHere'?s\s+[^.:\n]{0,60}:",
 'precision-theater':       r'\b(?:exactly|precisely)\s+(?:one|two|three|\d+)\b',
 'flourish-into':           r'\bdrops?\s+you\s+(?:straight|right)\s+into\b',
}

FORMATTING = {
 'em/en dash':        r'[—–]',
 'curly quotes':      r'[“”‘’]',
 'bold-colon list':   r'^\s*[-*•]\s*\*\*[^*]+\*\*\s*:',
 'emoji':             r'[\U0001F000-\U0001FAFF☀-⛿✀-➿]',
 'excessive bold':    None,   # counted separately
}

# Tells that only exist relative to the destination format: markdown syntax
# inside a BBCode post means a model emitted its native format and nobody
# converted it. Checked only when the file actually carries BBCode tags.
BBCODE_ONLY = {
 'markdown link in bbcode':   r'\[[^\]]+\]\((?:https?|www)[^)]*\)',
 'markdown heading in bbcode': r'^#{1,6}\s+\S',
 'bbcode inline-header list': r'\[\*\]\s*\[b\][^\[]+\[/b\]\s*[:—–-]',
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

def title_case_headings(raw):
    # Title Case In Headings: every main word capitalized. Wikipedia lists it
    # as a formatting tell; sentence case is the house style.
    heads = re.findall(r'^#{1,6}\s+(.+)$', raw, flags=re.M)
    heads += re.findall(r'\[b\]([^\[]{8,60})\[/b\]', raw, flags=re.I)
    bad = []
    for h in heads:
        words = re.findall(r"[A-Za-z][A-Za-z'-]*", h)
        big = [w for w in words if len(w) >= 4]
        # Titlecase-shaped only (Word, not WORD): all-caps runs are Forth
        # words and acronyms in reference tables, not a formatting tell.
        if len(words) >= 3 and len(big) >= 2 \
           and all(w[0].isupper() and w[1:].islower() for w in big):
            bad.append(h.strip())
    return bad

def formula_outside_code(prose):
    # 2026-08-27 (pretty-print r6): an 84-word typeable expression sat
    # inline in a sentence; Stan moved it to [code]. Machine input in
    # prose is a formatting decision made wrong, not "unavoidable".
    hits = []
    for tok in prose.split():
        if '[' in tok or 'http' in tok:
            continue
        if len(tok) >= 25 and re.search(r'[();=]', tok):
            hits.append(tok)
    return hits

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
    hard = 0
    for name, pat in HARD.items():
        hits = re.findall(pat, raw, flags=re.I)
        if hits:
            hard += len(hits)
            ex = re.search(pat, raw, flags=re.I)
            ctx = raw[max(0, ex.start()-30):ex.end()+30].replace('\n', ' ').strip()
            print(f"  [HARD] {name}: {len(hits)}  e.g. ...{ctx}...")
    if hard:
        total += hard
        print(f"  [HARD] {hard} disqualifying hit(s) — not judgment calls; the text was not read after generation")
    for name, pat in FORMATTING.items():
        if pat is None: continue
        hits = re.findall(pat, raw, flags=re.M)
        if hits:
            total += len(hits)
            print(f"  [FORMAT] {name}: {len(hits)}")
    if re.search(r'\[(b|code|list|url)\]', raw, flags=re.I):
        for name, pat in BBCODE_ONLY.items():
            hits = re.findall(pat, raw, flags=re.M)
            if hits:
                total += len(hits)
                print(f"  [FORMAT] {name}: {len(hits)}")
    tc = title_case_headings(raw)
    if tc:
        total += len(tc)
        print(f"  [FORMAT] title-case heading: {len(tc)}  e.g. \"{tc[0][:60]}\"")
    b = len(re.findall(r'\*\*[^*]+\*\*', raw)) + len(re.findall(r'\[b\]', raw, flags=re.I))
    if b > 6:
        total += 1
        print(f"  [FORMAT] heavy bold/emphasis: {b} spans")
    r3 = rule_of_three(prose)
    if r3:
        total += len(r3)
        print(f"  [CONSTRUCTION] rule-of-three: {len(r3)}  e.g. \"{r3[0][:70]}\"")
    foc = formula_outside_code(prose)
    if foc:
        total += len(foc)
        print(f"  [FORMAT] formula-outside-code: {len(foc)}  e.g. \"{foc[0][:60]}\" — typeable input belongs in [code]")
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
