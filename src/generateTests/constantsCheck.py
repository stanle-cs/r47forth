#!/usr/bin/env python3
# Stage 3 of the conversion verification: checks the CONSTANT VALUES themselves, the one thing
# generateTests.py cannot check because test and calculator share generateConstants.c.
#
# Method: every conversion constant is recomputed here INDEPENDENTLY, from the defining
# documents of the units (exact legal/SI definitions where they exist, referenced conventions
# where they do not), at 90-digit decimal precision - always more digits than any stored
# constant, so a verdict reads "Correct to NN verified digits" over ALL stored digits. The
# formulas and comments in generateConstants.c and any calculator output are deliberately NOT
# used for the reference values (they ARE copied into the report for comparison). Direction
# conventions (XtoY vs YtoX) are absorbed by also comparing against the reciprocal.
#
# The report src/generated/constantsVerification.txt is self-contained: assumptions, the full
# numbered source registry, then per constant the reference value with [n] citations, the
# compiled C47 literal with its C comment (both #if options where two exist), and the verdict.
# Constants that differ and unreviewed conventions land in a decision list at the top.
#
# Dev process for adding a constant: see 'Adding a constant' in conversions.md. In short: a
# new constant used by the conversion tables MUST get a REF entry here with an independent
# source; this script aborts the discipline gap loudly (UNREFERENCED in the decision list).
#
# Usage: runs AUTOMATICALLY in every testSuite build (invoked by generateTests.py after test
# generation; a nonzero exit fails the build). Manual run for iteration:
#   python3 src/generateTests/constantsCheck.py   (exit 1 while decisions are open)
import os
import re
import sys
from decimal import Decimal as D, Context, getcontext, ROUND_HALF_EVEN

sys.dont_write_bytecode = True                               # no __pycache__ from the import

HERE   = os.path.dirname(os.path.abspath(__file__))
SRC    = os.path.normpath(os.path.join(HERE, '..'))
OUTPUT = f'{SRC}/generated/constantsVerification.txt'

getcontext().prec = 90
getcontext().rounding = ROUND_HALF_EVEN

#**************************************************************************************************
#* Primitive exact definitions - each is the defining document's value, not a computation
#**************************************************************************************************
IN   = D('0.0254')            # metre per inch [4]
FT   = IN * 12                # foot = 12 in [4]
YD   = FT * 3                 # yard = 3 ft [4]
MI   = YD * 1760              # mile = 1760 yd = 1609.344 m [4]
NMI  = D('1852')              # nautical mile in m [18]
LB   = D('0.45359237')        # kg per pound [4]
GRAIN= LB / 7000              # grain = lb/7000 [4]
G0   = D('9.80665')           # standard gravity m/s^2 [6]
LBF  = LB * G0                # pound-force in N [2]
GALUS= D('231') * IN**3       # US gallon = 231 in^3 in m^3 [3]
GALUK= D('0.00454609')        # imperial gallon = 4.54609 L in m^3 [5]
L    = D('0.001')             # litre in m^3 [1]
ATM  = D('101325')            # standard atmosphere in Pa [6]
KB   = D('1.380649e-23')      # Boltzmann constant J/K, exact [7]
EV   = D('1.602176634e-19')   # elementary charge C = eV in J, exact [7]
DAY  = D('86400')             # day in s [1]
SFT  = D('1200') / D('3937')  # US survey foot in m [21]
CHI  = D('1') / 3             # PRC market chi in m [14]
PI   = D('3.1415926535897932384626433832795028841971693993751058209749445923078164062862'
         '089986280348253421170679')                        # pi to 100 decimals [15]

#**************************************************************************************************
#* ASSUMPTIONS - printed verbatim at the top of the report
#**************************************************************************************************
ASSUMPTIONS = [
 'A1  The C47/R47 constants are copied from src/generateConstants/generateConstants.c, taking',
 '    the #if branch actually compiled per the defines in src/c47/defines.h. Where a constant',
 '    is defined in both branches, both are shown.',
 'A2  Direction: a constant may be stored as X-to-Y or Y-to-X; a value matching the RECIPROCAL',
 '    of the reference is counted as correct and marked so.',
 'A3  mmHg/inHg: C47 KEEPS the BS 350:2004 abbreviated convention (133.3224 Pa) as its',
 '    standard (decision 2026-07-10), selected via MMHG_PA_133_3224 = 1 [16]; the #else',
 '    branch holds the conventional (non-abbreviated) values 133.322387415 /',
 '    3386.388640341 Pa [17].',
 'A4  banana: no external standard exists. The C47 value bananamm = 178 mm is taken as the',
 '    DEFINING convention and is therefore correct by definition; bananaInch is verified',
 '    against it (178/25.4).',
 'A5  beard-second: 10 nm (the Google Calculator convention) is assumed; a 5 nm variant is',
 '    also documented [12].',
 'A6  UK kitchen measures: imperial tablespoon = 5/8 imp floz, teaspoon = tbsp/3, cup = 10',
 '    imp floz (pre-metric convention) [13]; the modern metric 15/5 ml spoons differ.',
 'A7  US cup: the customary cup = 8 US floz = 236.5882365 ml [3]; the US legal cup of 240 ml',
 '    differs.',
 'A8  firkin (FFF system): 90 lb exactly, the mass of a firkin (9 imperial gallons) of water',
 '    at 10 lb/gal [11].',
 'A9  Gasoline gallon equivalent: 33.7 kWh per US gallon, the US EPA window-sticker MPGe',
 '    factor [10].',
]

#**************************************************************************************************
#* REFERENCES - the numbered source registry printed in the report; [n] tags below cite these
#**************************************************************************************************
REFERENCES = [
 '[1]  BIPM, The International System of Units (SI Brochure), 9th edition 2019: defining',
 '     constants Table 1 p130; non-SI accepted units (min/h/d, au, ha, L, t, mmHg) Table 8',
 '     pp145-146 - https://www.bipm.org/en/publications/si-brochure (accessed 2026-07-10)',
 '[2]  NIST Special Publication 811 (2008 ed.), Guide for the Use of the SI: conversion',
 '     factors Appendix B, alphabetical list B.8 pp49-67 -',
 '     https://physics.nist.gov/cuu/pdf/sp811.pdf (accessed 2026-07-10)',
 '[3]  NIST Handbook 44, Appendix C "General Tables of Units of Measurement", pp C1-C17',
 '     (customary length/mass/volume, cup/tbsp/tsp, hp electric) -',
 '     https://www.nist.gov/pml/owm/nist-handbook-44-current-edition (accessed 2026-07-10)',
 '[4]  International Yard and Pound Agreement 1959: 1 yd = 0.9144 m, 1 lb = 0.45359237 kg',
 '     exactly; US Federal Register Doc. 59-5442, 24 FR 5348 (1959-07-01); tabulated in [2]',
 '     B.8 and [3] App. C p C2/C8',
 '[5]  UK Weights and Measures Act 1985 (c.72), Schedule 1 "Definitions of units of',
 '     measurement": gallon = 4.54609 cubic decimetres exactly -',
 '     https://www.legislation.gov.uk/ukpga/1985/72/schedule/1 (accessed 2026-07-10)',
 '[6]  CGPM resolutions: 3rd CGPM 1901 p70 (g0 = 9.80665 m/s^2), 10th CGPM 1954 Resolution 4',
 '     (atm = 101325 Pa), 4th CGPM 1907 p105 (metric carat = 0.2 g) -',
 '     https://www.bipm.org/en/committees/cg/cgpm/resolutions (accessed 2026-07-10)',
 '[7]  SI 2019 defining constants (26th CGPM 2018, Resolution 1): kB = 1.380649e-23 J/K and',
 '     e = 1.602176634e-19 C, both exact; NIST SP 330 (2019 ed.) Table 1 p4; also [1] Table 1',
 '     p130',
 '[8]  IAU Resolution 2012 B2 (au = 149597870700 m exactly); IAU Resolution 2015 B2 Table 1',
 '     (pc = 648000/pi au exactly); light-year = c x Julian year (365.25 d), c = 299792458',
 '     m/s [1] - https://www.iau.org/administration/resolutions/general_assemblies/',
 '     (accessed 2026-07-10)',
 '[9]  ISO 31-4:1992 Annex B (superseded by ISO 80000-5): Btu(IT) = 1055.05585262 J exactly;',
 '     calorie(IT) = 4.1868 J exactly (5th Int. Conference on the Properties of Steam,',
 '     London 1956); also [2] B.8 p52 (Btu(IT)), p53 (cal(IT))',
 '[23] ISO 80000-3 (Quantities and units - Space and time) / IEC 80000-13: level of a power',
 '     quantity in dB uses the decade factor 10, level of a field (root-power) quantity the',
 '     factor 20',
 '[24] NPL (National Physical Laboratory, UK) 1998, p13 footnote, on mercury-column pressure',
 '     conventions - identified only as cited by the C source comment of MmhgToPa/InhgToPa;',
 '     the exact NPL document title is not recorded in the repository',
 '[10] US EPA fuel economy labeling (MPGe), 40 CFR Part 600: 33.7 kWh per gallon of gasoline',
 '     (window-sticker convention) - https://www.epa.gov/fueleconomy (accessed 2026-07-10)',
 '[11] Wikipedia: FFF system, section "Units" (firkin = 90 lb) - permanent link',
 '     https://en.wikipedia.org/w/index.php?title=FFF_system&oldid=1343014422',
 '     (accessed 2026-07-10)',
 '[12] Wikipedia: List of humorous units of measurement, section "Beard-second" (5 nm',
 '     physicist variant, 10 nm Google Calculator variant) - permanent link',
 '     https://en.wikipedia.org/w/index.php?title=List_of_humorous_units_of_measurement'
     '&oldid=1356149592 (accessed 2026-07-10)',
 '[13] Wikipedia: Cooking weights and measures, table "British (imperial) measures" -',
 '     permanent link https://en.wikipedia.org/w/index.php?title=Cooking_weights_and_measures'
     '&oldid=1355681534 (accessed 2026-07-10)',
 '[14] Chinese market units (shizhi), State Council 1959: chi = 1/3 m, jin = 0.5 kg;',
 '     Wikipedia: Chinese units of measurement, section "People\'s Republic of China" -',
 '     permanent link https://en.wikipedia.org/w/index.php?title=Chinese_units_of_measurement'
     '&oldid=1357247228 (accessed 2026-07-10)',
 '[15] pi, OEIS A000796 - https://oeis.org/A000796 (accessed 2026-07-10)',
 '[16] BS 350:2004 Conversion factors for units, p51: mmHg = 133.3224 Pa (abbreviated',
 '     convention; also cited by the C source comment and NPL 1998 p13 footnote)',
 '[17] Conventional mercury column: rho_Hg = 13.5951 g/cm^3 at 0 degC x g0 = 9.80665 m/s^2,',
 '     product exactly 133.322387415 Pa; [2] B.8 p58 lists the rounded 1.333224e2',
 '[18] International Extraordinary Hydrographic Conference, Monaco 1929: 1 nmi = 1852 m;',
 '     also [1] Table 8 p145 and [2] B.8 p59',
 '[19] DTP/PostScript point = 1/72 in; Wikipedia: Point (typography), section "Desktop',
 '     publishing point" - permanent link',
 '     https://en.wikipedia.org/w/index.php?title=Point_(typography)&oldid=1361729836',
 '     (accessed 2026-07-10)',
 '[20] Gregorian calendar mean year = 365.2425 d: leap rule 97/400, papal bull Inter',
 '     gravissimas (1582)',
 '[21] US survey foot = 1200/3937 m exactly (retained by the 1959 agreement [4] for surveys)',
 '     - https://www.nist.gov/pml/us-surveyfoot (accessed 2026-07-10)',
 '[22] Wikipedia: Foe (unit) (1 foe = 1e51 erg = 1e44 J) - permanent link',
 '     https://en.wikipedia.org/w/index.php?title=Foe_(unit)&oldid=1360897827',
 '     (accessed 2026-07-10)',
]

#**************************************************************************************************
#* REF - name (as in generateConstants.c) -> (reference value, description with [n] citations)
#**************************************************************************************************
REF = {
  # mathematics and temperature anchors
  '180onPi':      (D(180) / PI,          'degrees per radian = 180/pi [15]'),
  '200onPi':      (D(200) / PI,          'gradians per radian = 200/pi [15]'),
  '9on10':        (D(9) / 10,            'gradian per degree = 9/10; deg and grad definitions [1]'),
  '9on5':         (D(9) / 5,             'Fahrenheit per Celsius degree = 9/5 [2]'),
  '32':           (D(32),                '0 degC = 32 degF; Fahrenheit scale definition [2]'),
  '273p15':       (D('273.15'),          '0 degC = 273.15 K; kelvin definition [1]'),
  '459p67':       (D('459.67'),          '0 degF = 459.67 degR; Rankine scale [2]'),
  '10':           (D(10),                'dB power-quantity decade factor [23]'),
  '20':           (D(20),                'dB field-quantity decade factor [23]'),
  '100':          (D(100),               'definitional integer of the per-100 fuel economy '
                                         'unit family (no external source applicable)'),
  '1e_12':        (D('1e-12'),           'metric prefix scale [1]'),
  '1000':         (D(1000),              'tonne = 1000 kg [1]'),
  '10000':        (D(10000),             'hectare = 10^4 m^2 [1]'),
  '2':            (D(2),                 'jin = 0.5 kg, so 2 jin/kg [14]'),
  '3':            (D(3),                 'chi = 1/3 m, so 3 chi/m [14]'),
  'e':            (EV,                   'elementary charge = eV in J, exact [7]'),
  'kBeVK':        (KB / EV,              'kB/e, both exact [7]'),
  'gEarth':       (G0,                   'standard gravity 9.80665 m/s^2 [6]'),

  # length
  'InchToM':      (IN,                   'in = 0.0254 m exactly [4]'),
  'InchToCm':     (IN * 100,             'in = 2.54 cm exactly [4]'),
  'InchToMm':     (IN * 1000,            'in = 25.4 mm exactly [4]'),
  'FtToM':        (FT,                   'ft = 0.3048 m exactly [4]'),
  'YardToM':      (YD,                   'yd = 0.9144 m exactly [4]'),
  'MiToM':        (MI,                   'mi = 1609.344 m exactly [4]'),
  'MiToKm':       (MI / 1000,            'mi = 1.609344 km exactly [4]'),
  'NmiToM':       (NMI,                  'nmi = 1852 m exactly [18]'),
  'NmiToKm':      (NMI / 1000,           'nmi = 1.852 km exactly [18]'),
  'NmiToMi':      (NMI / MI,             'nmi/mi = 1852/1609.344 [18][4]'),
  'FathomToM':    (FT * 6,               'fathom = 6 ft [3]'),
  'furToM':       (YD * 220,             'furlong = 220 yd = 201.168 m [3]'),
  'SfeetToM':     (SFT,                  'US survey foot = 1200/3937 m exactly [21]'),
  'PointToMm':    (IN * 1000 / 72,       'DTP point = 1/72 in [19]'),
  'AuToM':        (D('149597870700'),    'au, exact [8]'),
  'LyToM':        (D('299792458') * D('31557600'), 'ly = c x Julian year [8]'),
  'PcToM':        (D(648000) / PI * D('149597870700'), 'pc = 648000/pi au exactly [8]'),
  'CunToM':       (CHI / 10,             'cun = chi/10 [14]'),
  'FenToM':       (CHI / 100,            'fen = chi/100 [14]'),
  'ZhangToM':     (CHI * 10,             'zhang = 10 chi [14]'),
  'YinToM':       (CHI * 100,            'yin = 10 zhang [14]'),
  'LiToM':        (CHI * 1500,           'li = 1500 chi = 500 m [14]'),
  'brdsTom':      (D('1e-8'),            'beard-second = 10 nm, assumption A5 [12]'),
  'brdsToIn':     (D('1e-8') / IN,       'beard-second / inch, assumption A5 [12][4]'),

  # area
  'Ft2ToM2':      (FT**2,                'ft^2 = 0.09290304 m^2 exactly [4]'),
  'Ft2ToHa':      (FT**2 / 10000,        'ft^2 in hectare [4][1]'),
  'AccreToHa':    (YD**2 * 4840 / 10000, 'acre = 4840 yd^2 [3]'),
  'AccreusToHa':  (SFT**2 * 43560 / 10000, 'US survey acre = 43560 survey ft^2 [21][3]'),
  'MiSqToKmSq':   ((MI / 1000)**2,       'mi^2 in km^2 [4]'),
  'NmiSqToKmSq':  ((NMI / 1000)**2,      'nmi^2 in km^2 [18]'),
  'MuToM2':       (CHI**2 * 6000,        'mu = 6000 chi^2 = 2000/3 m^2 [14]'),
  'In2ToMm2':     ((IN * 1000)**2,       'in^2 = 645.16 mm^2 exactly [4]'),

  # volume
  'In3ToMm3':     ((IN * 1000)**3,       'in^3 = 16387.064 mm^3 exactly [4]'),
  'In4ToMm4':     ((IN * 1000)**4,       'in^4 (second moment) = 25.4^4 mm^4 [4]'),
  'In6ToMm6':     ((IN * 1000)**6,       'in^6 = 25.4^6 mm^6 [4]'),
  'In3Ml':        (IN**3 / L * 1000,     'in^3 = 16.387064 ml exactly [4]'),
  'Ft3L':         (FT**3 / L,            'ft^3 = 28.316846592 L exactly [4]'),
  'Ft3Gluk':      (FT**3 / GALUK,        'ft^3 per imperial gallon [4][5]'),
  'Ft3ToGalUS':   (FT**3 / GALUS,        'ft^3 per US gallon = 1728/231 [4][3]'),
  'GalusToL':     (GALUS / L,            'US gal = 231 in^3 = 3.785411784 L exactly [3][4]'),
  'GalukToL':     (GALUK / L,            'imperial gal = 4.54609 L exactly [5]'),
  'GlukFzuk':     (D(160),               'imperial gallon = 160 imp floz [5]'),
  'GlusFzus':     (D(128),               'US gallon = 128 US floz [3]'),
  'BarrelToM3':   (GALUS * 42,           'oil barrel = 42 US gal [3]'),
  'QuartToL':     (GALUK / 4 / L,        'imperial quart = gal/4 = 1.1365225 L [5]'),
  'QtMl':         (GALUK / 4 / L * 1000, 'imperial quart in ml [5]'),
  'QtusMl':       (GALUS / 4 / L * 1000, 'US liquid quart = 946.352946 ml [3]'),
  'LQtus':        (GALUS / 4 / L,        'US liquid quart in L [3]'),
  'PintukMl':     (GALUK / 8 / L * 1000, 'imperial pint = 568.26125 ml [5]'),
  'PintlqMl':     (GALUS / 8 / L * 1000, 'US liquid pint = 473.176473 ml [3]'),
  'FlozukToMl':   (GALUK / 160 / L * 1000, 'imperial floz = gal/160 = 28.4130625 ml [5]'),
  'FlozusToMl':   (GALUS / 128 / L * 1000, 'US floz = gal/128 = 29.5735295625 ml [3]'),
  'FlozukToIn3':  (GALUK / 160 / IN**3,  'imperial floz in in^3 [5][4]'),
  'FlozusToIn3':  (GALUS / 128 / IN**3,  'US floz = 231/128 in^3 exactly [3][4]'),
  'CupcMl':       (GALUS / 16 / L * 1000, 'US customary cup = 8 floz = 236.5882365 ml, '
                                          'assumption A7 [3]'),
  'CupcFzus':     (D(8),                 'US cup = 8 US floz, assumption A7 [3]'),
  'CupukMl':      (GALUK / 16 / L * 1000, 'imperial cup = 10 imp floz = 284.130625 ml, '
                                          'assumption A6 [13]'),
  'CupukFzuk':    (D(10),                'imperial cup = 10 imp floz, assumption A6 [13]'),
  'TbspcMl':      (GALUS / 256 / L * 1000, 'US tbsp = cup/16 = 14.78676478125 ml [3]'),
  'TspcMl':       (GALUS / 768 / L * 1000, 'US tsp = tbsp/3 = 4.92892159375 ml [3]'),
  'FzusTbspc':    (D(2),                 'US floz = 2 tbsp [3]'),
  'FzusTspc':     (D(6),                 'US floz = 6 tsp [3]'),
  'TbspukMl':     (GALUK / 256 / L * 1000, 'imperial tbsp = 5/8 imp floz = 17.7581640625 ml, '
                                           'assumption A6 [13]'),
  'TspukMl':      (GALUK / 768 / L * 1000, 'imperial tsp = tbsp/3 ml, assumption A6 [13]'),
  'FzukTbspuk':   (D(8) / 5,             'imp floz = 1.6 imperial tbsp, assumption A6 [13]'),
  'FzukTspuk':    (D(24) / 5,            'imp floz = 4.8 imperial tsp, assumption A6 [13]'),

  # mass
  'LbToKg':       (LB,                   'lb = 0.45359237 kg exactly [4]'),
  'OzToG':        (LB / 16 * 1000,       'oz = lb/16 = 28.349523125 g exactly [4]'),
  'TrozToG':      (GRAIN * 480 * 1000,   'troy oz = 480 grains = 31.1034768 g exactly [3][4]'),
  'CaratToG':     (D('0.2'),             'metric carat = 0.2 g exactly [6]'),
  'StoneToKg':    (LB * 14,              'stone = 14 lb [3]'),
  'CwtToKg':      (LB * 112,             'long (UK) hundredweight = 112 lb [3]'),
  'ShortcwtToKg': (LB * 100,             'short (US) hundredweight = 100 lb [3]'),
  'LongtonToKg':  (LB * 2240,            'long ton = 2240 lb [3]'),
  'ShorttonToKg': (LB * 2000,            'short ton = 2000 lb [3]'),
  'SlugToKg':     (LBF / FT,             'slug = lbf s^2/ft [2]'),
  'SlinchToKg':   (LBF / IN,             'slinch (blob) = lbf s^2/in = 12 slug [2]'),
  'BlobInLbs':    (G0 / IN,              'blob mass in lb = g0/in, dimensionless [2][6][4]'),
  'firToKg':      (LB * 90,              'FFF firkin = 90 lb, assumption A8 [11][4]'),
  'firToLb':      (D(90),                'FFF firkin = 90 lb exactly, assumption A8 [11]'),

  # force, torque, line density
  'LbfToN':       (LBF,                  'lbf = 4.4482216152605 N exactly [2][4][6]'),
  'LbfftToNm':    (LBF * FT,             'lbf ft in N m [2][4]'),
  'InlbsToNm':    (LBF * IN,             'lbf in in N m [2][4]'),
  'LbsftToKgm':   (LB / FT,              'lb/ft in kg/m [4]'),

  # density
  'Lbsin3ToKgm3': (LB / IN**3,           'lb/in^3 in kg/m^3 [4]'),
  'Lbsin3ToTmm3': (LB / IN**3 * D('1e-12'), 'lb/in^3 in t/mm^3 [4][1]'),
  'Kgm3ToBlobin3':(LBF / IN / IN**3,     'blob/in^3 = lbf s^2 in^-4 in kg/m^3 [2][4][6]'),

  # pressure
  'AtmToPa':      (ATM,                  'atm = 101325 Pa exactly [6]'),
  'BarToPa':      (D('1e5'),             'bar = 100000 Pa exactly [1]'),
  'TorrToPa':     (ATM / 760,            'torr = 101325/760 Pa exactly [2][6]'),
  'MmhgToPa':     (D('133.3224'),        'mmHg = 133.3224 Pa, the BS 350:2004 convention '
                                         'KEPT as the C47 standard [16], assumption A3; '
                                         'conventional 133.322387415 [17] is the #else option'),
  'InhgToPa':     (D('133.3224') * D('25.4'), 'inHg = 133.3224 x 25.4 = 3386.38896 Pa, BS '
                                         '350:2004 convention KEPT as the C47 standard [16], '
                                         'assumption A3; conventional [17] is the #else option'),
  'PsiToPa':      (LBF / IN**2,          'psi = lbf/in^2 [2][4][6]'),
  'KsiToMpa':     (LBF / IN**2 / 1000,   'ksi = 1000 psi, in MPa [2]'),
  'Lbsft2ToPa':   (LBF / FT**2,          'lbf/ft^2 in Pa [2][4]'),

  # energy
  'WhToJ':        (D(3600),              'Wh = 3600 J exactly [1]'),
  'CalToJ':       (D('4.1868'),          'IT calorie = 4.1868 J exactly [9]'),
  'BtuToJ':       (D('1055.05585262'),   'IT Btu = 1055.05585262 J exactly [9]'),
  'ErgToJ':       (D('1e-7'),            'erg = 1e-7 J exactly; CGS [2]'),
  'FoeToJ':       (D('1e44'),            'foe (bethe) = 1e51 erg = 1e44 J [22]'),
  'GaluseqE':     (D('33.7'),            'gasoline gallon equivalent = 33.7 kWh, '
                                         'assumption A9 [10]'),

  # power
  'HpmToW':       (G0 * 75,              'metric hp = 75 kgf m/s = 735.49875 W exactly [2][6]'),
  'HpukToW':      (LBF * FT * 550,       'mechanical hp = 550 ft lbf/s W [2][4][6]'),
  'HpeToW':       (D(746),               'electrical hp = 746 W exactly [3]'),

  # speed
  'Mphmps':       (MI / 3600,            'mph = 0.44704 m/s exactly [4]'),
  'Kmphmps':      (D(1000) / 3600,       'km/h = 1/3.6 m/s exactly [1]'),
  'KnotToMps':    (NMI / 3600,           'knot = 1852/3600 m/s exactly [18]'),
  'MphToKnot':    (MI / NMI,             'mph/knot = 1609.344/1852 [4][18]'),
  'MphToFps':     (D(5280) / 3600,       'mph in ft/s = 22/15 exactly [4]'),
  'fpsToMps':     (FT,                   'ft/s = 0.3048 m/s exactly [4]'),
  'fpsToKph':     (FT * D('3.6'),        'ft/s = 1.09728 km/h exactly [4]'),
  'ftnToS':       (DAY * 14,             'fortnight = 14 d = 1209600 s [1]'),
  'fpfToMps':     (YD * 220 / (DAY * 14), 'furlong/fortnight = 201.168/1209600 m/s [3][4][1]'),
  'fpfToKph':     (YD * 220 / (DAY * 14) * D('3.6'), 'furlong/fortnight in km/h [3][4][1]'),
  'fpfToMph':     (YD * 220 / (DAY * 14) / (MI / 3600), 'furlong/fortnight in mph = '
                                         '(201.168/1209600)/0.44704 [3][4][1]'),

  # angular speed
  'RpmDegps':     (D(6),                 'rpm = 360/60 = 6 deg/s exactly [1]'),
  'RpmRadps':     (PI / 30,              'rpm = 2 pi/60 = pi/30 rad/s [15]'),

  # time
  'YearToS':      (D('365.2425') * DAY,  'Gregorian mean year = 365.2425 d = 31556952 s [20]'),
}

# convention anchors: the C47 value IS the definition (assumption A4); verified by declaration,
# and other constants may be derived from them (bananaInch below)
CONVENTION_ANCHORS = {
  'bananamm': 'the banana length; C47 defines the convention (assumption A4)',
}

# per-constant extra evidence lines, shown under the verdict
NOTES = {
  'InhgToPa': 'note       see assumption A3; the #else option holds the conventional value '
              '[17] = 13.5951 x 9.80665 x 25.4 exactly.',
  'MmhgToPa': 'note       see assumption A3; the #else option holds the conventional value '
              '[17] = 13.5951 x 9.80665 exactly.',
}

#**************************************************************************************************
#* load_c47_constants
#* Purpose:     literal, source line comment, C identifier and #if-branch status per constant
#* Source:      src/generateConstants/generateConstants.c; defines from src/c47/defines.h
#* Destination: two dicts: active name -> (literal, comment, identifier), and alternates
#*              name -> [(literal, comment), ...] from branches NOT compiled; the identifier
#*              is the C symbol: const_Name for EXACT, const39_Name for APPROX
#* Function:    matches generateConstant("Name", digits, EXACT|APPROX, "+value") capturing the
#*              trailing // comment verbatim; #if (NAME == n) evaluated per defines.h exactly
#*              as the compiler does, other #if forms stay active in both branches
#**************************************************************************************************
def load_c47_constants():
  defines = {}
  for line in open(f'{SRC}/c47/defines.h', encoding='utf-8'):
    m = re.match(r'#define\s+(\w+)\s+(\d+)', line)
    if m:
      defines[m.group(1)] = int(m.group(2))
  consts, alternates = {}, {}
  active = [(True, False)]
  for line in open(f'{SRC}/generateConstants/generateConstants.c', encoding='utf-8'):
    if re.match(r'\s*#elif', line):
      sys.exit('generateConstants.c uses #elif; extend this parser (see generateTests.py).')
    if re.match(r'\s*#if', line):
      m = re.match(r'\s*#if\s*\((\w+)\s*==\s*(\d+)\)', line)
      if m:
        active.append((defines.get(m.group(1)) == int(m.group(2)), True))
      else:
        active.append((True, False))
      continue
    if re.match(r'\s*#else', line):
      state, evaluable = active[-1]
      if evaluable:
        active[-1] = (not state, True)
      continue
    if re.match(r'\s*#endif', line):
      active.pop()
      continue
    m = re.search(r'generateConstant\("([^"]+)",\s*\d+,\s*(\w+),\s*"([+-][0-9.eE+-]+)"\s*\)\s*;'
                  r'\s*(//.*)?$', line)
    if m:
      prefix = 'const39_' if m.group(2) == 'APPROX' else 'const_'
      if all(state for state, evaluable in active):
        consts[m.group(1)] = (m.group(3), (m.group(4) or '').strip(), prefix + m.group(1))
      else:
        alternates.setdefault(m.group(1), []).append((m.group(3), (m.group(4) or '').strip()))
  return consts, alternates

#**************************************************************************************************
#* input_fingerprints
#* Purpose:     pin the exact audited inputs and repository state into the record
#* Source:      the two parsed C files, git (read-only queries), the system date
#* Destination: list of header lines: date, commit + dirty flag, remote URL, SHA-256 of each
#*              input file
#* Function:    hashlib over the raw file bytes; git queried via subprocess, 'unavailable' on
#*              any failure so the record still generates outside a git checkout
#**************************************************************************************************
def input_fingerprints():
  import datetime
  import hashlib
  import subprocess
  lines = [f'Generated:  {datetime.datetime.now(datetime.timezone.utc).date().isoformat()} '
           f'(UTC date; the input hashes below pin the exact audited state)']
  root = os.path.normpath(os.path.join(SRC, '..'))
  def git(*args):
    try:
      return subprocess.run(['git', '-C', root] + list(args), capture_output=True,
                            text=True, timeout=10).stdout.strip()
    except Exception:
      return ''
  commit = git('rev-parse', 'HEAD') or 'unavailable'
  dirty  = ' (working tree had uncommitted changes)' if git('status', '--porcelain') else ''
  lines.append(f'Repository: {git("remote", "get-url", "origin") or "unavailable"}')
  lines.append(f'Commit:     {commit}{dirty}')
  lines.append('            (note: the committed copy of this file is necessarily one commit')
  lines.append('            ahead of the recorded hash)')
  for path in (f'{SRC}/generateConstants/generateConstants.c', f'{SRC}/c47/defines.h'):
    digest = hashlib.sha256(open(path, 'rb').read()).hexdigest()
    lines.append(f'Input:      {os.path.relpath(path, root)}')
    lines.append(f'            SHA-256 {digest}')
  return lines

#**************************************************************************************************
#* used_constants
#* Purpose:     which constants the conversion machinery actually uses
#* Source:      generateTests.load_all() tables and the conversion function bodies in
#*              src/c47/conversionUnits.c
#* Destination: set of names as in generateConstants.c (prefixes stripped)
#* Function:    union of the conversionFactors slots reachable from convertPairs UNIT_CONV
#*              items, the cvtTempConsts coefficients, and const_ tokens inside the FNS
#*              formula function bodies; const_0/const_1 skip markers excluded
#**************************************************************************************************
def used_constants():
  sys.path.insert(0, HERE)
  import generateTests as gt
  Dt = gt.load_all()
  used = set()
  for row in Dt['rows']:
    fn, arg, left = Dt['items'][row['item']]
    if fn == 'fnUnitConvert':
      used.add(Dt['slots'][arg[0]])
  for r in Dt['temp_rows']:
    used.update(r)
  src = open(f'{SRC}/c47/conversionUnits.c', encoding='utf-8').read()
  for fname in gt.FNS.split('|'):
    m = re.search(r'void ' + fname + r'\b.*?\n\}', src, re.S)
    if m:
      used.update(re.findall(r'\bconst(?:39|51)?_\w+', m.group(0)))
  return {re.sub(r'^const(?:39|51)?_', '', u) for u in used} - {'0', '1'}

#**************************************************************************************************
#* matching_digits
#* Purpose:     how many leading significant digits two values share
#* Source:      the reference value and the C47 literal, as Decimals
#* Destination: 0..80; caps above any stored constant length
#* Function:    rounds both to n significant digits, half even, largest agreeing n wins
#**************************************************************************************************
def matching_digits(ref, val):
  for n in range(80, 0, -1):
    c = Context(prec=n, rounding=ROUND_HALF_EVEN)
    if c.plus(ref) == c.plus(val):
      return n
  return 0

#**************************************************************************************************
#* sig_digits
#* Purpose:     the number of significant digits stored in a C47 literal
#* Source:      the literal string, e.g. '+1.609344' or '+3.386388640341e+03'
#* Destination: integer count of coefficient digits, trailing zeros included
#* Function:    strips sign, exponent and decimal point, drops leading zeros only
#**************************************************************************************************
def sig_digits(literal):
  mantissa = re.split('[eE]', literal.lstrip('+-'))[0].replace('.', '')
  return len(mantissa.lstrip('0')) or 1

#**************************************************************************************************
#* refstr
#* Purpose:     render a reference value for the report
#* Source:      a Decimal at 90-digit working precision
#* Destination: normalised string at 60 significant digits, more than any stored constant
#* Function:    Context(prec=60).plus, then normalize and str
#**************************************************************************************************
def refstr(x):
  return str(Context(prec=60, rounding=ROUND_HALF_EVEN).plus(x).normalize())

#**************************************************************************************************
#* main
#* Purpose:     compare every used constant against the compiled literal, write the report
#* Source:      REF/CONVENTION_ANCHORS/NOTES above, generateConstants.c, the used set
#* Destination: src/generated/constantsVerification.txt; exit 1 when any decision is open
#* Function:    per constant: verified-digit agreement, reciprocal (direction convention)
#*              agreement, convention anchors correct by declaration, or a difference;
#*              used constants without a REF entry are flagged UNREFERENCED (dev process)
#**************************************************************************************************
def main():
  c47, alternates = load_c47_constants()
  refs = dict(REF)
  if 'bananamm' in c47:                                      # assumption A4: banana anchor
    refs['bananaInch'] = (D(c47['bananamm'][0]) / D('25.4'),
                          'banana (178 mm, assumption A4) / inch [4]')
  entries, decisions = [], []
  for name in sorted(set(refs) | CONVENTION_ANCHORS.keys(), key=str.lower):
    if name not in c47:
      if name == '1e_12':
        continue                                             # const_1e_* synthesized, no literal
      decisions.append(f'{name}: REF entry exists but generateConstants.c has no such '
                       f'constant - fix the REF table')
      continue
    literal, comment, identifier = c47[name]
    stored = sig_digits(literal)
    lines = [identifier]
    if name in CONVENTION_ANCHORS:
      lines.append(f'  reference  {"DEFINED-BY-C47":55s} {CONVENTION_ANCHORS[name]}')
      lines.append(f'  C47        {literal:55s} {comment}')
      lines.append('  verdict    Correct by convention: this value IS the definition '
                   '(assumption A4)')
    else:
      ref, source = refs[name]
      lines.append(f'  reference  {refstr(ref):55s} {source}')
      lines.append(f'  C47        {literal:55s} {comment}')
      for alt_literal, alt_comment in alternates.get(name, []):
        lines.append(f'  C47 #else  {alt_literal:55s} {alt_comment}')
      value = D(literal)
      n  = matching_digits(ref, value)
      nr = matching_digits(D(1) / ref, value) if ref != 0 else 0
      digitword = 'digit' if stored == 1 else 'digits'
      if n >= stored:
        lines.append(f'  verdict    Correct to {stored} verified {digitword}')
      elif nr >= stored:
        lines.append(f'  verdict    Correct to {stored} verified {digitword} as the '
                     f'RECIPROCAL (assumption A2)')
      else:
        best, how = (n, '') if n >= nr else (nr, ' as the RECIPROCAL (assumption A2)')
        lines.append(f'  verdict    DIFFERS from digit {best + 1} of {stored}{how} '
                     f'- decision needed')
        decisions.append(f'{name}: C47 {literal} vs reference {refstr(ref)} '
                         f'({source}); agrees to {best} digit(s){how}')
    if name in NOTES:
      lines.append(f'  {NOTES[name]}')
    entries.append('\n'.join(lines))
  unreferenced = sorted(used_constants() - set(refs) - set(CONVENTION_ANCHORS))
  for name in unreferenced:
    decisions.append(f'{name}: used by the conversion tables but has NO REF entry - add one '
                     f'with an independent source (see "Adding a constant" in conversions.md)')
  body = '\n\n'.join(entries)
  import hashlib
  body_sha = hashlib.sha256(body.encode('utf-8')).hexdigest()
  out = []
  out.append('INDEPENDENT AUDIT of the C47/R47 conversion constants - provenance record')
  out.append('=' * 100)
  out.append('Project:    C47/R47, community firmware for HP-42/DM42-class scientific')
  out.append('            calculators (this repository)')
  out.extend(input_fingerprints())
  out.append('Generator:  src/generateTests/constantsCheck.py; regenerate with')
  out.append('            python3 src/generateTests/constantsCheck.py   (exit 1 while any')
  out.append('            decision is open). Environment: Python 3, stdlib decimal only.')
  out.append('Encoding:   UTF-8; non-ASCII occurs only inside quoted C source comments.')
  out.append('            Lines up to 255 characters are intentional (verbatim quotes and')
  out.append('            full-precision reference lines); do not re-wrap.')
  out.append('')
  out.append('THIS DOCUMENT IS AN AUDIT. Every conversion constant DECLARED in the C source')
  out.append('file src/generateConstants/generateConstants.c (the #if branch actually')
  out.append('compiled, per src/c47/defines.h) is checked against a reference value computed')
  out.append('INDEPENDENTLY from the defining documents of each unit. The C47 lines below are')
  out.append('the audited declarations, quoted for comparison only; they contribute nothing')
  out.append('to the reference values. No calculator output is used anywhere.')
  out.append('')
  out.append('METHOD')
  out.append('Reference values are computed with 90-digit decimal arithmetic, rounding half')
  out.append('even, and displayed to at most 60 significant digits. A verdict "Correct to N')
  out.append('verified digits" means: N is the number of significant digits stored in the C47')
  out.append('literal (trailing zeros counted, so +100 stores 3 digits while +1e+03 stores 1),')
  out.append('and rounding the reference to N significant digits (half even) reproduces the')
  out.append('stored value exactly - a correctly rounded final stored digit therefore counts')
  out.append('as verified.')
  out.append('SCOPE: although high precision is stated throughout (90-digit references,')
  out.append('declarations of up to 55 digits), the calculator rounds every constant to, and')
  out.append('computes with, 34 significant digits (real34) at runtime. That is immaterial to')
  out.append('this document, which audits the raw source declarations, not their runtime use.')
  out.append('')
  out.append('VERDICTS - the complete vocabulary this generator can emit:')
  out.append('  "Correct to N verified digit(s)"                 every stored digit verified')
  out.append('  "... as the RECIPROCAL (assumption A2)"          verified against 1/reference')
  out.append('  "Correct by convention: ..."                     the C47 value IS the')
  out.append('                                                   definition (assumption A4)')
  out.append('  "DIFFERS from digit n of m - decision needed"    mismatch; also listed under')
  out.append('                                                   DECISIONS NEEDED')
  out.append('An "open decision" is any DIFFERS verdict, any convention awaiting review, any')
  out.append('constant used by the conversion tables without a REF entry (UNREFERENCED), or')
  out.append('any REF entry without a matching declaration. A "note" line may add evidence or')
  out.append('context to a verdict without changing it.')
  out.append('')
  out.append('RECORD FORMAT - one block per constant, sorted case-insensitively by constant')
  out.append('name (the part after the const_/const39_ prefix); the block anchor is the full')
  out.append('C identifier:')
  out.append('  reference  <value or DEFINED-BY-C47> <derivation with [n] and An citations>')
  out.append('  C47        <declared literal>        <the C source comment, quoted VERBATIM,')
  out.append('                                        errors included; it played no part in')
  out.append('                                        the verification>')
  out.append('  C47 #else  <the literal of the branch NOT compiled, where one exists>')
  out.append('  verdict    <one phrase from VERDICTS above>')
  out.append('  note       <optional evidence or cross-reference>')
  out.append('The first whitespace-delimited token after each field name is the value.')
  out.append('Assumption A1 applies to every entry; A2 wherever RECIPROCAL appears.')
  out.append('')
  out.append('ASSUMPTIONS')
  out.extend(ASSUMPTIONS)
  out.append('')
  out.append('REFERENCES')
  out.extend(REFERENCES)
  out.append('')
  out.append(f'{len(entries)} conversion constants checked; {len(decisions)} open '
             f'decision(s).')
  out.append('')
  if decisions:
    out.append('DECISIONS NEEDED (meeting list):')
    for d in decisions:
      out.append(f'  - {d}')
    out.append('')
  out.append('=' * 100)
  out.append('')
  out.append(body)
  out.append('')
  out.append('=' * 100)
  out.append(f'END OF RECORD - {len(entries)} entries; SHA-256 of the entry section between')
  out.append(f'the ruled lines: {body_sha}')
  tmp = OUTPUT + '.tmp'                                      # atomic: no torn record on disk
  with open(tmp, 'w', encoding='utf-8', newline='\n') as f:
    f.write('\n'.join(out) + '\n')
  os.replace(tmp, OUTPUT)
  print(f'{len(entries)} constants verified, {len(decisions)} decision(s) needed, '
        f'written to {OUTPUT}')
  return 1 if decisions else 0

if __name__ == '__main__':
  sys.exit(main())
