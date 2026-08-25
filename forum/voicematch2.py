#!/usr/bin/env python3
"""voicematch2 — measure Stan-ness PRESENCE, not AI-tell absence.

Built 2026-08-25 after the undo-history v0.1 rejection ("the framework
didn't appropriately capture my own writing style, from the data saved").
The old voicematch compares aggregate rates; a draft can match every rate
and carry none of his constructions. This tool checks a draft for the
constructions the corpus shows he actually uses, and reports which are
missing. It reads the corpus live so the reference is the data, never a
baked-in number.

Usage: python3 forum/voicematch2.py <draft> [--register release|chat]

  release: list/changelog sections ([list]..[/list], [code]..) are held to
           the v0.3 structure rulings (impersonal, no questions) and only
           the PROSE between them is scored for voice presence.
  chat:    the whole text is scored (replies, comments).
"""
import io, re, sys, statistics

CORPUS = "forum/reference/reddit-trungdle.md"

CONSTRUCTIONS = [
    # key, corpus regex, human name, expected-in-prose floor per ~600 words
    ("i_hedge",   r"\bI (?:think|guess|would|tend to|hope|found|feel|mean)\b|\bIn my opinion\b|\bMy personal\b", "first-person hedge (I think / I'd / in my opinion)", 2),
    ("direct_you",r"\byou (?:can|need|should|have to|want|get|just)\b|\bYou just\b", "direct-you instruction", 2),
    ("hedge_adv", r"\b(?:maybe|probably|likely|kinda|pretty much|somewhat)\b", "hedging adverb beside the claim", 1),
    ("fronted",   r"(?:^|[.!?]\s+)(?:But|So|And|Also|Anyways?|Btw)\b", "fronted connector (But/So/And/Also/Anyways/Btw)", 2),
    ("question",  r"\?", "question (his explanations ask them)", 1),
    ("paren",     r"\([^)]{3,90}\)", "parenthetical aside carrying a fact or reason", 2),
    ("analogy",   r"\b(?:like a|just like|similar to|same as|think of)\b", "everyday comparison", 1),
    ("intens",    r"\b(?:really|very|super|totally|absolutely|surprisingly)\b", "intensifier / enthusiasm", 1),
    ("concede",   r"\b(?:I'm sure|Of course|True,|Agreed?,|Fair)\b.*?\bbut\b", "concession then position", 0),
    ("stake",     r"\bI (?:made|built|added|put|keep|use|wrote|haven't|had to|skip|chose)\b", "first-person stake in the thing", 1),
]

def strip_markup(t, release):
    if release:
        t = re.sub(r"\[list\].*?\[/list\]", " ", t, flags=re.S)
    t = re.sub(r"\[code\].*?\[/code\]", " ", t, flags=re.S)
    t = re.sub(r"\[attachment[^\]]*\].*?\[/attachment\]", " ", t, flags=re.S)
    t = re.sub(r"\[[^\]]{1,20}\]", " ", t)
    return t

def skeleton(s):
    keep = set("""a an the and or but so if then than as of in on at to for from with without by is are was were be been being do does did done have has had will would can could should may might must not no this that these those it its he she they you i we what which who when where how""".split())
    toks = re.findall(r"[a-z']+", s.lower())
    return " ".join(t if t in keep else "_" for t in toks)

def main():
    args = sys.argv[1:]
    register = "release"
    if "--register" in args:
        i = args.index("--register"); register = args[i+1]; del args[i:i+2]
    draft_path = args[0]
    draft_raw = io.open(draft_path, encoding="utf-8").read()
    prose = strip_markup(draft_raw, register == "release")
    words = len(prose.split())

    raw = io.open(CORPUS, encoding="utf-8").read().split("## Comments", 1)[1]
    texts = []
    for e in re.split(r"\n### ", raw):
        t = "\n".join(l for l in e.split("\n")[1:] if not l.startswith("<http")).strip()
        if t: texts.append(t)
    corpus = "\n".join(texts)
    cwords = len(corpus.split())

    print(f"draft prose words: {words}   register: {register}\n")
    print(f"{'construction':52s} {'corpus/100w':>11s} {'draft/100w':>10s} {'draft n':>7s}  verdict")
    missing = 0
    for key, rx, name, floor in CONSTRUCTIONS:
        cn = len(re.findall(rx, corpus))
        dn = len(re.findall(rx, prose))
        crate = cn * 100.0 / cwords
        drate = dn * 100.0 / words if words else 0
        expected = max(floor, round(crate * words / 100))
        verdict = "ok"
        if dn == 0 and crate > 0.02 and floor > 0:
            verdict = "MISSING"; missing += 1
        elif dn < floor:
            verdict = "thin"
        print(f"{name:52s} {crate:11.3f} {drate:10.3f} {dn:7d}  {verdict}")

    sents = [s.strip() for s in re.split(r"(?<=[.!?])\s+", prose) if len(s.split()) >= 4]
    csk = set()
    for s in re.split(r"(?<=[.!?])\s+", corpus):
        if len(s.split()) >= 4:
            csk.add(skeleton(s))
    unsupported = []
    for s in sents:
        sk = skeleton(s)
        if sk not in csk:
            # relaxed: any corpus skeleton sharing >=70% tokens counts
            st = sk.split()
            hit = any(len(set(st) & set(c.split())) >= 0.7 * len(st) for c in csk if abs(len(c.split()) - len(st)) <= 3)
            if not hit:
                unsupported.append(s)
    print(f"\nsentence skeletons without close corpus support: {len(unsupported)}/{len(sents)}")
    for s in unsupported[:8]:
        print("   ??", s[:110])

    if missing:
        print(f"\nVERDICT: {missing} of his signature constructions absent from the prose. It will not sound like him regardless of scanner results.")
    else:
        print("\nVERDICT: construction presence ok. Stan's read still decides.")

if __name__ == "__main__":
    main()
