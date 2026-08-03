# Regenerating the AN0029 screen captures

Every picture in the note is a real capture from the GTK simulator, driven by the same keystrokes the
note prints. Nothing is mocked up or retouched; the only processing is a 3x point-filter upscale so
the pixels stay square in print.

## Build

```
git worktree add ../C47-wt-an29 master
cd ../C47-wt-an29 && make sim
```

`./c47` chdirs to its own folder, so the `.bmp` files land beside the binary.

## Capture

`--reset` starts from factory defaults and ignores `backup.cfg`, which is what makes these
reproducible on any machine. `snap <name>` writes `<name>.bmp`.

| shot | command |
| --- | --- |
| 01-home | `./c47 --reset --exec 'snap s01home'` |
| 02-rpn | `./c47 --reset --exec 'press "@k 30"; press "@k 12"; press "@k 23"; press "@k 36"; snap s02add'` |
| 03-region | `./c47 --reset --exec 'press @f; press "@k 15"; press "@k 22"; snap d2'` |
| 04-mode | `./c47 --reset --exec 'press @f; press "@k 14"; snap s04mode'` |
| 05-cplx | `./c47 --reset --exec 'press "@k 28"; press @g; press "@k 09"; press "@k 29"; press "@k 12"; press "@k 30"; press @g; press "@k 09"; press "@k 23"; press "@k 26"; snap s07cmul'` |
| 06-catalog | `./c47 --reset --exec 'press @f; press "@k 36"; snap p4cat'` |
| 07-func | `./c47 --reset --script func.t47` |
| 08-mvar, 09-solve | `./c47 --reset --script solve.t47` |
| 10-wave | `./c47 --reset --snapskiprefresh --script wave.t47` |
| 11-stat | `./c47 --reset --script stat.t47` |
| 12-yhat | `./c47 --reset --script stat2.t47` |
| 13-prgm, 14-io | `./c47 --reset --script pgms.t47`, `./c47 --reset --exec 'press @g; press "@k 31"; snap p3io'` |

A plot needs `--snapskiprefresh`, otherwise `snap` redraws the screen and wipes the drawn pixels.

## Scale for print

```
magick <name>.bmp -filter point -resize 300% screens/<nn>-<name>.png
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

`press @f` and `press @g` set the pending shift; `press F1`..`F6` press the soft keys.

## Firmware floor

The graphing walkthrough needs commit c1e432718, "fix(grapher): let PGMPLT program plots run without
a formula". Without it a program plot launched from the MVAR menu stops with "No equation defined".
That fix is in master, so the whole set reproduces on a stock build.
