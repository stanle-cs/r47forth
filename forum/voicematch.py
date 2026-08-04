#!/usr/bin/env python3
"""Compare a draft's measurable voice against the reference corpus.

Usage: python3 forum/voicematch.py <draft> [corpus]
Corpus defaults to forum/reference/reddit-trungdle.md. Reports side-by-side
numbers; the judgment stays human, same doctrine as the other scanners.
"""
import re, sys, statistics

CORPUS_DEFAULT = 'forum/reference/reddit-trungdle.md'

def prose_of(path, is_corpus=False):
    t = open(path, encoding='utf-8').read()
    if is_corpus:
        # strip headers, links, code fences from the corpus file
        t = re.sub(r'^#.*$', ' ', t, flags=re.M)
        t = re.sub(r'<http[^>]*>', ' ', t)
        t = re.sub(r'```.*?```', ' ', t, flags=re.S)
    t = re.sub(r'\[code\].*?\[/code\]', ' ', t, flags=re.S | re.I)
    t = re.sub(r'\[/?\w+[^\]]*\]', ' ', t)
    t = re.sub(r'`[^`]+`', ' ', t)
    t = re.sub(r'https?://\S+', ' ', t)
    return t

def sentences(t):
    t = re.sub(r'\s+', ' ', t)
    parts = re.split(r'(?<=[.!?])\s+', t)
    return [p.strip() for p in parts if len(p.split()) >= 2]

CONTR = re.compile(r"\b\w+'(s|t|ll|re|ve|d|m)\b", re.I)
FIRST = re.compile(r"\b(i|i'm|i'll|i've|i'd|my|me|mine)\b", re.I)
SECOND = re.compile(r"\b(you|you're|you'll|you've|your|yours)\b", re.I)
OPENERS = ['i', 'you', 'the', 'a', 'it', "it's", 'there', "there's", 'but',
           'and', 'so', 'if', 'this', 'that', 'my', 'for', 'when', 'not']
CONNECT = ['but', 'so', 'though', 'however', 'also', 'actually', 'probably',
           'maybe', 'anyway', 'honestly', 'basically', 'just']

def profile(t):
    s = sentences(t)
    words = t.split()
    n = max(len(words), 1)
    L = [len(x.split()) for x in s] or [0]
    p = {
        'sentences': len(s),
        'mean_len': statistics.mean(L),
        'cv': (statistics.pstdev(L) / statistics.mean(L)) if statistics.mean(L) else 0,
        'contractions_per_100w': 100 * len(CONTR.findall(t)) / n,
        'first_person_per_100w': 100 * len(FIRST.findall(t)) / n,
        'second_person_per_100w': 100 * len(SECOND.findall(t)) / n,
        'questions_pct': 100 * sum(1 for x in s if x.endswith('?')) / max(len(s), 1),
    }
    op = {}
    for x in s:
        w = re.sub(r'^\W+', '', x.split()[0].lower()) if x.split() else ''
        if w in OPENERS:
            op[w] = op.get(w, 0) + 1
    p['openers'] = {k: 100 * v / max(len(s), 1) for k, v in
                    sorted(op.items(), key=lambda kv: -kv[1])}
    low = t.lower()
    p['connectors_per_100w'] = {c: 100 * len(re.findall(r'\b' + c + r'\b', low)) / n
                                for c in CONNECT}
    return p

def show(name, a, b):
    print(f"\n{'':24}  {'CORPUS':>8}  {'DRAFT':>8}")
    for k in ('mean_len', 'cv', 'contractions_per_100w', 'first_person_per_100w',
              'second_person_per_100w', 'questions_pct'):
        print(f"{k:24}  {a[k]:8.2f}  {b[k]:8.2f}")
    print("\nsentence openers (% of sentences):")
    keys = sorted(set(a['openers']) | set(b['openers']),
                  key=lambda k: -(a['openers'].get(k, 0)))
    for k in keys[:12]:
        print(f"  {k:12} {a['openers'].get(k, 0):6.1f}  {b['openers'].get(k, 0):6.1f}")
    print("\nconnectors (per 100 words):")
    for k in CONNECT:
        ca, cb = a['connectors_per_100w'][k], b['connectors_per_100w'][k]
        if ca > 0.02 or cb > 0.02:
            print(f"  {k:12} {ca:6.2f}  {cb:6.2f}")

if __name__ == '__main__':
    draft = sys.argv[1]
    corpus = sys.argv[2] if len(sys.argv) > 2 else CORPUS_DEFAULT
    show(draft, profile(prose_of(corpus, is_corpus=True)),
         profile(prose_of(draft)))
