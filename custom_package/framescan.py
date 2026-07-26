#!/usr/bin/env python3
"""Pattern-agnostic tell finder.

The phrase-list approach can only catch tells someone already named. This one
derives them from the text: content words are blanked out, leaving a skeleton of
function words and punctuation, and any skeleton that RECURS is a formulaic
frame. "no X, no Y" surfaces as "no . , no ." without anyone having to think of
it first.

Also flags shapes that are generically machine-ish regardless of wording:
coordinate negation series, colon-explanations, semicolons, and repeated
sentence openers/closers.
"""
import re, sys, collections

FUNC = set("""a an the this that these those and or but nor so yet for
of in on at by to from with without into onto over under after before during
is are was were be been being am do does did done have has had having
it its it's they them their there here what which who whom whose when where why how
not no never nothing neither none nor only just also too very much many more most
if then else than as such same other another each every both all any some few
you your we our i my me he she him her his hers one ones
can could may might must shall should will would
up down out off again still yet already ever between across through about
per via like unlike within beyond upon""".split())

PUNCT = set(list(",.;:!?()[]-—–\"'"))

def strip_code(t):
    t = re.sub(r'\[code\].*?\[/code\]', ' ', t, flags=re.S | re.I)
    t = re.sub(r'```.*?```', ' ', t, flags=re.S)
    t = re.sub(r'^(?: {4}|\t).*$', ' ', t, flags=re.M)
    t = re.sub(r'\[/?\w+[^\]]*\]', ' ', t)
    t = re.sub(r'`[^`]+`', ' ', t)
    return t

def sentences(t):
    t = re.sub(r'\s+', ' ', t)
    return [s for s in re.split(r'(?<=[.!?])\s+(?=[A-Z(])', t) if len(s.split()) >= 4]

def tokens(s):
    return re.findall(r"[A-Za-z][A-Za-z'’-]*|[,.;:!?]", s)

def skel(s):
    out = []
    for w in tokens(s):
        lw = w.lower()
        out.append(lw if (lw in FUNC or w in PUNCT) else '.')
    return out

def scan(paths):
    sents = []
    for p in paths:
        for s in sentences(strip_code(open(p, encoding='utf-8').read())):
            sents.append((p.split('/')[-1], s))

    print(f"corpus: {len(sents)} sentences across {len(paths)} files\n")

    # --- recurring function-word frames -------------------------------------
    frames = collections.defaultdict(list)
    for src, s in sents:
        sk = skel(s)
        for n in (4, 5, 6):
            for i in range(len(sk) - n + 1):
                g = sk[i:i + n]
                if sum(1 for x in g if x != '.') < 2:   # need real structure
                    continue
                if g[0] == '.' and g[-1] == '.':
                    continue
                frames[' '.join(g)].append((src, s))
    print("== recurring frames (same function-word skeleton, 2+ uses) ==")
    hits = [(k, v) for k, v in frames.items() if len(v) >= 2]
    hits.sort(key=lambda kv: (-len(kv[1]), -len(kv[0])))
    seen_sent = set()
    shown = 0
    for k, v in hits:
        ids = {id(s) for _, s in v}
        if ids <= seen_sent:
            continue
        seen_sent |= ids
        print(f"  [{len(v)}x] \"{k}\"")
        for src, s in v[:3]:
            print(f"        {src}: ...{s[:88]}")
        shown += 1
        if shown >= 12:
            break
    if not shown:
        print("  none")

    # --- generic machine-ish shapes -----------------------------------------
    print("\n== coordinate negation series (the \"no X, no Y\" shape) ==")
    neg = re.compile(r'\b(no|not|never|nothing|neither)\b', re.I)
    found = 0
    for src, s in sents:
        if len(neg.findall(s)) >= 2 and (',' in s or ' or ' in s):
            print(f"  {src}: {s[:100]}")
            found += 1
    # sentence fragments too (headings/lists split by '.')
    for p in paths:
        for frag in re.split(r'(?<=[.!?])\s+', strip_code(open(p, encoding='utf-8').read())):
            f = frag.strip()
            if len(neg.findall(f)) >= 2 and ',' in f and len(f.split()) < 14:
                print(f"  {p.split('/')[-1]} (fragment): {f[:100]}")
                found += 1
    if not found:
        print("  none")

    print("\n== other shape counts ==")
    body = ' '.join(s for _, s in sents)
    print(f"  colon-explanations  : {len(re.findall(r'[a-z]\s*:\s+[a-z]', body))}")
    print(f"  semicolons          : {body.count(';')}")
    op = collections.Counter(s.split()[0].lower() for _, s in sents)
    print(f"  repeated openers    : {[(w, c) for w, c in op.most_common(5) if c > 2] or 'none'}")
    cl = collections.Counter(' '.join(s.rstrip('.').split()[-2:]).lower() for _, s in sents)
    print(f"  repeated closers    : {[(w, c) for w, c in cl.most_common(4) if c > 1] or 'none'}")

if __name__ == '__main__':
    scan(sys.argv[1:])
