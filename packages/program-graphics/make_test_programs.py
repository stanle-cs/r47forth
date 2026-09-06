#!/usr/bin/env python3
"""
Generate .p47 and .r47 test program files for C47 / R47 firmware testing.
"""
import os
import sys

def encode_item(item_id):
    if item_id < 128:
        return [item_id]
    return [(item_id >> 8) | 0x80, item_id & 0xFF]

def encode_literal_int(val):
    s = str(val).encode('ascii')
    return [114, 8, len(s)] + list(s)

def encode_literal_real(s_val):
    s = s_val.encode('ascii')
    return [114, 9, len(s)] + list(s)

def encode_literal_string(s_val):
    s = s_val.encode('utf-8')
    return [114, 253, len(s)] + list(s)

def encode_label(name):
    s = name.encode('ascii')
    return [1, 253, len(s)] + list(s)

def write_p47(filename, byte_list):
    # C47 format: PROGRAM length count excludes the trailing 255 255 .END. marker
    payload_len = len(byte_list) - 2 if byte_list[-2:] == [255, 255] else len(byte_list)
    content = ["PROGRAM_FILE_FORMAT", "0", "C47_program_file_version", "1", "PROGRAM", str(payload_len)]
    for b in byte_list:
        content.append(str(b))
    content.append("")
    text = "\n".join(content)
    with open(filename, "w", newline="\n") as f:
        f.write(text)

# Item opcodes
ITM_PVIEW     = 2448
ITM_ERASE     = 2449
ITM_LINE      = 2450
ITM_BOX       = 2451
ITM_FBOX      = 2452
ITM_CIRC      = 2453
ITM_FCIRC     = 2454
ITM_ARC       = 2455
ITM_TEXTOUT   = 2456
ITM_DISP      = 2457
ITM_GMODE     = 2458
ITM_GCLIP     = 2459
ITM_XRNG      = 2460
ITM_YRNG      = 2461
ITM_EYEPT     = 2864
ITM_XVOL      = 2865
ITM_YVOL      = 2866
ITM_ZVOL      = 2867
ITM_NUMX      = 2868
ITM_NUMY      = 2869
ITM_WIREFRAME = 2870
ITM_PT3D      = 2871
ITM_LINE3D    = 2872

ITM_RTN       = 4
ITM_STOP      = 70
ITM_XexY      = 36
ITM_SQUARE    = 58
ITM_SUB       = 96

END_MARKER    = [255, 255]

def make_programs(out_dir):
    os.makedirs(out_dir, exist_ok=True)

    # 1. TSTGFX: Didier's repro test (PVIEW 6, ERASE, 0 79 238 319 FBOX)
    tstgfx_bytes = []
    tstgfx_bytes += encode_label("TSTGFX")
    tstgfx_bytes += encode_item(ITM_PVIEW) + [6]
    tstgfx_bytes += encode_item(ITM_ERASE)
    tstgfx_bytes += encode_literal_int(0)
    tstgfx_bytes += encode_literal_int(79)
    tstgfx_bytes += encode_literal_int(238)
    tstgfx_bytes += encode_literal_int(319)
    tstgfx_bytes += encode_item(ITM_FBOX)
    tstgfx_bytes += END_MARKER

    # 2. SADL: 3D function z = x^2 - y^2
    sadl_bytes = []
    sadl_bytes += encode_label("SADL")
    sadl_bytes += encode_item(ITM_SQUARE)
    sadl_bytes += encode_item(ITM_XexY)
    sadl_bytes += encode_item(ITM_SQUARE)
    sadl_bytes += encode_item(ITM_SUB)
    sadl_bytes += encode_item(ITM_RTN)
    sadl_bytes += END_MARKER

    # 3. SURF: 3D wireframe plot of SADL on a 24x24 mesh (self-contained with SADL routine)
    surf_bytes = []
    surf_bytes += encode_label("SURF")
    surf_bytes += encode_item(ITM_PVIEW) + [6]
    surf_bytes += encode_item(ITM_ERASE)
    surf_bytes += encode_literal_real("-1")
    surf_bytes += encode_literal_real("1")
    surf_bytes += encode_item(ITM_XRNG)
    surf_bytes += encode_literal_real("-0.6")
    surf_bytes += encode_literal_real("0.6")
    surf_bytes += encode_item(ITM_YRNG)
    surf_bytes += encode_literal_int(0)
    surf_bytes += encode_literal_int(-3)
    surf_bytes += encode_literal_int(0)
    surf_bytes += encode_item(ITM_EYEPT)
    surf_bytes += encode_literal_int(24)
    surf_bytes += encode_item(ITM_NUMX)
    surf_bytes += encode_literal_int(24)
    surf_bytes += encode_item(ITM_NUMY)
    # WIREFRAME 'SADL'
    surf_bytes += encode_item(ITM_WIREFRAME) + [253, 4, ord('S'), ord('A'), ord('D'), ord('L')]
    surf_bytes += encode_item(ITM_STOP)
    surf_bytes += encode_item(ITM_RTN)
    # SADL function subroutine: z = x^2 - y^2
    surf_bytes += encode_label("SADL")
    surf_bytes += encode_item(ITM_SQUARE)
    surf_bytes += encode_item(ITM_XexY)
    surf_bytes += encode_item(ITM_SQUARE)
    surf_bytes += encode_item(ITM_SUB)
    surf_bytes += encode_item(ITM_RTN)
    surf_bytes += END_MARKER

    # 4. DEMO2D: 2D primitives showcase (circles, outline box, inverted cutout, text)
    demo2d_bytes = []
    demo2d_bytes += encode_label("DEMO2D")
    demo2d_bytes += encode_item(ITM_PVIEW) + [6]
    demo2d_bytes += encode_item(ITM_ERASE)
    # Outer border box (10, 10) to (390, 230)
    # BOX takes: T=y2, Z=x2, Y=y1, X=x1
    demo2d_bytes += encode_literal_int(10)
    demo2d_bytes += encode_literal_int(10)
    demo2d_bytes += encode_literal_int(230)
    demo2d_bytes += encode_literal_int(390)
    demo2d_bytes += encode_item(ITM_BOX)
    # Filled circle center (200, 120), radius 70
    # FCIRCL takes: Z=r, Y=cy, X=cx
    demo2d_bytes += encode_literal_int(70)
    demo2d_bytes += encode_literal_int(120)
    demo2d_bytes += encode_literal_int(200)
    demo2d_bytes += encode_item(ITM_FCIRC)
    # GMODE 2 (Invert / XOR)
    demo2d_bytes += encode_item(ITM_GMODE) + [2]
    # Inverted hole: radius 35 at (200, 120)
    demo2d_bytes += encode_literal_int(35)
    demo2d_bytes += encode_literal_int(120)
    demo2d_bytes += encode_literal_int(200)
    demo2d_bytes += encode_item(ITM_FCIRC)
    # Reset GMODE to 0
    demo2d_bytes += encode_item(ITM_GMODE) + [0]
    # DISP 1: header banner
    demo2d_bytes += encode_literal_string("R47 2D GRAPHICS DEMO")
    demo2d_bytes += encode_item(ITM_DISP) + [1]
    demo2d_bytes += encode_item(ITM_STOP)
    demo2d_bytes += END_MARKER

    # 5. CUBE3D: Interactive 3D wireframe cube
    cube_bytes = []
    cube_bytes += encode_label("CUBE3D")
    cube_bytes += encode_item(ITM_PVIEW) + [6]
    cube_bytes += encode_item(ITM_ERASE)
    cube_bytes += encode_literal_real("-1")
    cube_bytes += encode_literal_real("1")
    cube_bytes += encode_item(ITM_XRNG)
    cube_bytes += encode_literal_real("-0.6")
    cube_bytes += encode_literal_real("0.6")
    cube_bytes += encode_item(ITM_YRNG)
    cube_bytes += encode_literal_int(0)
    cube_bytes += encode_literal_int(-3)
    cube_bytes += encode_literal_int(0)
    cube_bytes += encode_item(ITM_EYEPT)
    # 3D points for cube: x, y, z
    # Bottom square:
    pts = [
        ("PT", -1, -1, -1),
        ("LN",  1, -1, -1),
        ("LN",  1,  1, -1),
        ("LN", -1,  1, -1),
        ("LN", -1, -1, -1),
        # Up to top square:
        ("LN", -1, -1,  1),
        ("LN",  1, -1,  1),
        ("LN",  1,  1,  1),
        ("LN", -1,  1,  1),
        ("LN", -1, -1,  1),
        # 3 remaining vertical pillars:
        ("PT",  1, -1, -1),
        ("LN",  1, -1,  1),
        ("PT",  1,  1, -1),
        ("LN",  1,  1,  1),
        ("PT", -1,  1, -1),
        ("LN", -1,  1,  1),
    ]
    for cmd, x, y, z in pts:
        cube_bytes += encode_literal_int(x)
        cube_bytes += encode_literal_int(y)
        cube_bytes += encode_literal_int(z)
        if cmd == "PT":
            cube_bytes += encode_item(ITM_PT3D)
        else:
            cube_bytes += encode_item(ITM_LINE3D)

    cube_bytes += encode_literal_string("3D CUBE - USE ARROWS TO ROTATE")
    cube_bytes += encode_item(ITM_DISP) + [1]
    cube_bytes += encode_item(ITM_STOP)
    cube_bytes += END_MARKER

    programs = {
        "TSTGFX": tstgfx_bytes,
        "SADL": sadl_bytes,
        "SURF": surf_bytes,
        "DEMO2D": demo2d_bytes,
        "CUBE3D": cube_bytes,
    }

    for name, bdata in programs.items():
        p47_path = os.path.join(out_dir, f"{name}.p47")
        r47_path = os.path.join(out_dir, f"{name}.r47")
        write_p47(p47_path, bdata)
        write_p47(r47_path, bdata)
        print(f"Generated {p47_path} ({len(bdata)} bytes) and {r47_path}")

if __name__ == "__main__":
    target_dir = sys.argv[1] if len(sys.argv) > 1 else "dist_programs"
    make_programs(target_dir)
