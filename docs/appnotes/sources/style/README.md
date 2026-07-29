# c47appnote.sty

The shared design language for C47/R47 application notes: page geometry, palette, keypress and screen
notation, callout blocks, listing and table styles, chart styles, running heads, the front and back
matter, and both keyboard maps.

It replaces the ~50 lines of definitions that AN0029, AN0030 and AN0031 each carried inline, with a
comment in each saying to consolidate them once all three had landed.

**AN0032 is the template and the reference.** Copy `../AN0032/` to start a new note, and read AN0032
itself for what every construct looks like and when to use it.

## Using it

A note's whole preamble:

```latex
\documentclass[canadian]{article}
\usepackage{c47appnote}

\notenumber{032}
\noteshorttitle{Template}
\notedate{2026-07-29}
\notetitle{The Application Note Template}
```

The author is not a front-matter field. Attribution lives once, in `\authorblock` at the back beside
the colophon.

## Building

The style file is one directory up from the note, so `TEXINPUTS` has to point at it. From the note's
own directory, twice, so the table of contents, the page count and the cross references settle:

```
TEXINPUTS=../style: lualatex AN-00NN-C47-R47-Application-Note-YYYY-MM-DD-XX-slug.ltx
TEXINPUTS=../style: lualatex AN-00NN-C47-R47-Application-Note-YYYY-MM-DD-XX-slug.ltx
```

The trailing colon keeps the default search path. Then copy the PDF to `docs/appnotes/` under the
published name, `AN00NN YYYY-MM-DD C47 R47 XX Short Title.pdf`.

## Requirements

- **lualatex.** The package calls `\RequireLuaTeX` and stops under pdflatex or xelatex.
- **TeX Live 2023 or later**, for TeX Gyre Heros, New Computer Modern Sans Math, tcolorbox, titlesec,
  tocloft, booktabs, pgfplots and unicode-math. Nothing else has to be installed.
- **JuliaMono**, from <https://juliamono.netlify.app>. This is the one font the package cannot supply
  from TeX Live. The `\screen` and `\prog` boxes reproduce the calculator display and need its glyphs
  literally. Without it the package falls back to DejaVu Sans Mono and warns during the build.

## Options

| Option | Effect |
| --- | --- |
| `nofonts` | Do not touch the fonts; the note sets its own. |
| `serifmath` | Leave mathematics in the default face instead of matching the sans body. |

## What it defines

| Group | Commands |
| --- | --- |
| Keys | `\key` `\fkey` `\gkey` `\mkey` `\fmkey` `\gmkey` `\lkey` |
| Display | `\screen` `\prog` `\seq` `\keys` |
| Captures | `\shot` `\shotpair` `\shotfig` |
| Callouts | `note` `tip` `warning` |
| Listings | styles `c47text`, `c47code`, `c47inline`; `\lit` |
| Tables | `ntable`, `\hd`, booktabs rules |
| Charts | `nchart`, styles `canaxis` `canA` `canB` `canC` `canaccent` `canpoints` `canbars` `canemph` `canrest` |
| Markup | `\cmd` `\vr` `\reg` `\menu` `\file` `\lsc` `\swatch` `\HW` `\TODO` |
| Front matter | `\notenumber` `\noteshorttitle` `\notedate` `\notetitle` `\notehead`, `revisions` + `\revision` |
| Back matter | `\colophon`, `\authorblock`, `\keyboardappendix`, `\ckeyboard` `\rkeyboard`, `\kc` `\shiftcell` |

## Colours

Seven colours, and two of them are spoken for. `fcol` (orange, `DE6E10`) means "reached through the f
shift" and `gcol` (blue, `2C6DB5`) means "reached through the g shift", everywhere, without exception:
not a heading, not a rule, not a running head, not a callout. Everything else is `keyink`, `soft`, or a
tint of `keyink`. `warnink` (`B3341F`) is the only accent, spent on the `warning` callout and on the
`\HW` and `\TODO` draft markers.

A filled panel is the calculator's own screen: only `\screen` and `\prog` carry the `lcdbg` wash, so a
quotation of the calculator never looks like a quotation of a file. Rules carry weight in one direction
only: 1pt masthead, 0.7pt table, 0.4pt every other hairline. No double rules, no vertical rules.

There is no separate callout for hardware differences. A feature a build does not carry, or a capture
that will not reproduce, costs the reader an hour, which is what a `warning` is for. Name the build in
the first sentence.

## Charts

**A chart tells its series apart by dash pattern and marker, not by hue.** `canA` solid, `canB` dashed,
`canC` dotted, in that fixed order, all in `keyink`. Magnitude uses the grey ramp `seq1`..`seq5`.
Emphasis is one mark in `keyink` against the rest in `seq4`. Never a second y axis.

Exactly **one** hue is available, `chartink` (`#00A099`), for the single series that is the point of the
chart. That is not a stylistic choice. A categorical palette has to clear a chroma floor, a lightness
band, a contrast floor and a pairwise separation floor of ΔE ≥ 15 in normal vision and ≥ 8 under
simulated protanopia and deuteranopia. Orange and blue are already spent on the shifts and red on the
warning accent, and against those three reserved inks:

| Candidate | Verdict | Why |
| --- | --- | --- |
| violet `#8A5CC7` | unusable | collapses against `gcol`, ΔE 3.4 under protanopia |
| ochre `#8A6B00` | unusable | collapses against `warnink`, ΔE 2.5 under deuteranopia |
| teal `#00918A` | unusable | chroma 0.089, below the 0.10 floor: reads as grey |
| teal `#00A099` | **passes** | clears `warnink`, `fcol` and `gcol` on every pair, in normal vision and under all three simulated deficiencies |

Read AN0032 section 10 for the worked examples. If you add a colour, re-run the check rather than
eyeballing it.
