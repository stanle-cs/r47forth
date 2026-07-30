# Regenerating the AN0032 screen captures

Every picture in the note is a real capture from the GTK simulator, driven by the same keystrokes the
note prints. Nothing is mocked up or retouched; the only processing is a 3x point-filter upscale so
the pixels stay square in print.

This file is part of the template. Copy it into a new note's `screens/scripts/` and replace the table.

## Build

```
git worktree add ../C47-wt-mynote master
cd ../C47-wt-mynote && make sim
```

`./c47` chdirs to its own folder, so the `.bmp` files land beside the binary.

## Capture

`--reset` starts from factory defaults and ignores `backup.cfg`, which is what makes these
reproducible on any machine. `snap <name>` writes `<name>.bmp`.

| shot | command |
| --- | --- |
| 01-home | `./c47 --reset --exec 'snap s1home'` |
| 02-result | `./c47 --reset --exec 'press "@k 30"; press "@k 12"; press "@k 23"; press "@k 36"; snap s2add'` |
| 03-disp | `./c47 --reset --exec 'press @f; press "@k 15"; snap s3disp'` |
| 04-mode | `./c47 --reset --exec 'press @f; press "@k 14"; snap s4mode'` |

A plot needs `--snapskiprefresh`, otherwise `snap` redraws the screen and wipes the drawn pixels.

## Scale for print

```
magick <name>.bmp -filter point -resize 300% screens/<nn>-<name>.png
```

## Checking a set still reproduces

After the firmware has moved, rerun the recipe and diff against what is committed. Zero differing
pixels is the answer you want.

```
./c47 --reset --exec 'press "@k 30"; press "@k 12"; press "@k 23"; press "@k 36"; snap r'
magick r.bmp -filter point -resize 300% /tmp/r.png
compare -metric AE /tmp/r.png screens/02-result.png null:
```

## Physical key numbers

`press "@k NN"` presses physical key NN, numbered row-major from the C47 table in
`src/c47/assign.c`. In alpha entry the same key types the letter in brackets.

```
00 Sigma+ [A]  01 1/x [B]  02 sqrt [C]  03 LOG [D]  04 LN [E]   05 XEQ [F]
06 STO [G]     07 RCL [H]  08 Rdown [I] 09 SIN [J]  10 COS [K]  11 TAN [L]
12 ENTER       13 x<>y [M] 14 +/- [N]   15 EEX [O]  16 backspace
17 up          18 7 [P]    19 8 [Q]     20 9 [R]    21 div [S]
22 down        23 4 [T]    24 5 [U]     25 6 [V]    26 mult [W]
27 f/g shift   28 1 [X]    29 2 [Y]     30 3 [Z]    31 minus
32 EXIT        33 0        34 .         35 R/S      36 plus [space]
```

`press @f` and `press @g` set the pending shift; `press F1`..`F6` press the soft keys. A longer
sequence goes in a `.t47` script in this directory rather than on the command line, driven with
`./c47 --reset --script myscript.t47`.
