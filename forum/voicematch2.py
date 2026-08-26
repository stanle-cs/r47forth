#!/usr/bin/env python3
"""voicematch2 — register-conditioned Stan-ness: presence floors AND caps.

Built 2026-08-25 after the undo-history v0.1 rejection; REBUILT same day
after the r2 rejection ("too many AI-generated tell tales slipped through").
v1's mistake: it set corpus-derived FLOORS and no caps, so a draft written
to the tool stuffed Reddit-argument constructions into a release post —
hypophora, ornamental enthusiasm, manufactured hedges. Constructions are
LICENSES, not quotas. This version scores per register:

  chat:    the Reddit-argument register. Floors from the corpus rates.
  release: posts under his name. Floors are minimal (stake, direct-you,
           parentheticals, one hedge), and CAPS bind: questions are
           FORBIDDEN in release prose (his v0.3 edit deleted both
           hypophora on sight, 2026-08-10; the r2 rejection 2026-08-25
           confirms the ruling covers ALL post prose, not just changelog
           sections), intensifier-enthusiasm and analogy are capped at
           one, concession at one. Over a cap = STUFFED, blocking, same
           severity as MISSING.

Usage: python3 forum/voicematch2.py <draft> [--register release|update|chat]

  update: changelog-led update posts (v0.3 shape). Thin prose owes NO
          floors (his published artifact has none of them); caps and the
          segue-question rule still bind.
Floors scale with prose length (the listed floor is per ~600 words).
"""
import io, math, re, sys, statistics

CORPUS = "forum/reference/reddit-trungdle.md"

# key, corpus regex, human name, chat floor /600w, release floor /600w, release cap
CONSTRUCTIONS = [
    ("i_hedge",   r"\bI(?:'m (?:not )?sure|'d)\b|\bI (?:think|guess|would|tend to|hope|found|feel|mean)\b|\bIn my opinion\b|\bMy personal\b", "first-person hedge (I think / I'm sure / I'd)", 2, 0, 3),
    ("direct_you",r"\b[Yy]ou (?:can|need|should|have to|want|get|just)\b", "direct-you instruction", 2, 1, None),
    ("hedge_adv", r"\b(?:maybe|probably|likely|kinda|pretty much|somewhat)\b", "hedging adverb beside the claim", 1, 1, None),
    ("fronted",   r"(?:^|[.!?]\s+)(?:But|So|And|Also|Anyways?|Btw)\b", "fronted connector (But/So/And/Also/Anyways/Btw)", 2, 1, None),
    ("question",  r"\?", "question / hypophora", 1, 0, 2),
    ("paren",     r"\([^)]{3,90}\)", "parenthetical aside carrying a fact or reason", 2, 2, None),
    ("analogy",   r"\b(?:like (?:a|an|the)\b|just like|similar to|same as|think of)|, like\b", "everyday comparison", 1, 0, 1),
    ("intens",    r"\b(?:really|very|super|totally|absolutely|surprisingly)\b", "intensifier / enthusiasm", 1, 0, 1),
    ("concede",   r"\b(?:I'm sure|Of course|True,|Agreed?,|Fair)\b.*?\bbut\b", "concession then position", 0, 0, 1),
    ("stake",     r"\bI(?:'ve)? (?:made|built|added|put|keep|use|wrote|haven't|had to|skip|chose)\b", "first-person stake in the thing", 1, 1, None),
]

CAP_EVIDENCE = {
    "question": "the published v0.3 post carries exactly two content-bearing reader questions; more is decoration",
    "intens":   "'surprisingly handy' in rejected r2 — enthusiasm as ornament",
    "analogy":  "one earns its place; two is decoration (r2 carried a twist-tailed one)",
    "concede":  "'I'm sure there are corners I haven't hit' closed rejected r2",
    "i_hedge":  "a hedge with no judgment under it is stuffing ('I think that's the fastest way', r2)",
}

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
    release = register in ("release", "update")
    prose = strip_markup(draft_raw, release)
    words = len(prose.split())

    raw = io.open(CORPUS, encoding="utf-8").read().split("## Comments", 1)[1]
    texts = []
    for e in re.split(r"\n### ", raw):
        t = "\n".join(l for l in e.split("\n")[1:] if not l.startswith("<http")).strip()
        if t: texts.append(t)
    corpus = "\n".join(texts)
    cwords = len(corpus.split())

    print(f"draft prose words: {words}   register: {register}\n")
    print(f"{'construction':52s} {'corpus/100w':>11s} {'draft n':>7s} {'floor':>5s} {'cap':>4s}  verdict")
    blocking = []
    for key, rx, name, cfloor, rfloor, rcap in CONSTRUCTIONS:
        cn = len(re.findall(rx, corpus))
        dn = len(re.findall(rx, prose))
        crate = cn * 100.0 / cwords
        floor = 0 if register == "update" else (rfloor if release else cfloor)
        cap = rcap if release else None
        eff_floor = round(floor * words / 600.0) if floor else 0
        verdict = "ok"
        if cap == 0 and dn > 0:
            verdict = "FORBIDDEN"; blocking.append((key, verdict))
        elif cap is not None and cap > 0 and dn > cap:
            verdict = "STUFFED"; blocking.append((key, verdict))
        elif dn == 0 and eff_floor > 0 and crate > 0.02:
            verdict = "MISSING"; blocking.append((key, verdict))
        elif dn < eff_floor:
            verdict = "thin"
        caps = "-" if cap is None else str(cap)
        print(f"{name:52s} {crate:11.3f} {dn:7d} {eff_floor:5d} {caps:>4s}  {verdict}")
        if verdict in ("FORBIDDEN", "STUFFED") and key in CAP_EVIDENCE:
            print(f"{'':52s} ^ {CAP_EVIDENCE[key]}")

    if release:
        # Segue-formula questions are forbidden in ANY position. The published
        # v0.3 post carries two content-bearing reader questions ("Why keep a
        # failed line in history?", "Where do the lines go?" — one even OPENS
        # a paragraph); what died to his read (v0.3 draft, r2) are the
        # presentational segues that frame a tour instead of asking anything.
        SEGUE = r"(?:^|[.!?]\s+)So what\b[^?]{0,60}\?|\bWhat about\b[^?]{0,60}\?|\bwhat (?:are|do) you (?:looking at|seeing|getting)\b|\b(?:might|may) be (?:wondering|asking)\b"
        for m in re.finditer(SEGUE, prose):
            blocking.append(("question", "FORBIDDEN"))
            print(f"\nSEGUE QUESTION (presentational, frames instead of asks) — the shape that died to his read:")
            print(f"   ?? {re.sub(r'\s+', ' ', m.group(0)).strip()[:100]}")

    sents = [s.strip() for s in re.split(r"(?<=[.!?])\s+", prose) if len(s.split()) >= 4]
    csk = set()
    for s in re.split(r"(?<=[.!?])\s+", corpus):
        if len(s.split()) >= 4:
            csk.add(skeleton(s))
    unsupported = []
    for s in sents:
        sk = skeleton(s)
        if sk not in csk:
            st = sk.split()
            hit = any(len(set(st) & set(c.split())) >= 0.7 * len(st) for c in csk if abs(len(c.split()) - len(st)) <= 3)
            if not hit:
                unsupported.append(s)
    print(f"\nsentence skeletons without close corpus support: {len(unsupported)}/{len(sents)} (informational only)")
    for s in unsupported[:8]:
        print("   ??", s[:110])

    if blocking:
        stuffed = [k for k, v in blocking if v in ("FORBIDDEN", "STUFFED")]
        absent  = [k for k, v in blocking if v == "MISSING"]
        print(f"\nVERDICT: BLOCKING ({len(blocking)}).", end=" ")
        if stuffed: print(f"Stuffed past his release register: {', '.join(stuffed)}.", end=" ")
        if absent:  print(f"Signature constructions absent: {', '.join(absent)}.", end=" ")
        print("\nConstructions are licenses, not quotas — fix the draft, not the numbers.")
    else:
        print("\nVERDICT: construction presence and register caps ok. Stan's read still decides.")

if __name__ == "__main__":
    main()
