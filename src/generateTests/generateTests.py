#!/usr/bin/env python3
# Generates src/testSuite/tests/conversions.txt: one test per convertPairs[] item, 2 identical passes, every expected value computed here.
# Runs at build time via meson, same hook as generateConstants, so the test set always matches the sources it was built from.
# Also generates conversionsSI.txt: per configurable item the two SI round-trip halves (runConversionToSI / runConversionFromSI,
# the machinery behind custom conversion pairs) run via the testSuite wrappers covConvToSI / covConvFromSI and are compared
# against values computed here. Before writing, the convertPairs[] table itself is sanity checked; a bad table aborts the build
# with a message saying exactly what to fix.
#
# Reads only the C sources; computes each conversion of input 1 with decimal arithmetic at the calculator's precisions (each operation in a
# 39-digit context, result rounded half even to the 34-digit register). Calculator output is never read, so the testSuite checks the calculator
# against an independent computation of the same definitions.
# 
# NOT covered: the constant values themselves. Test and calculator share generateConstants.c, so a wrong literal there passes both.
# Verifying constants would need an independent source. That is not done here.
#
# Usage:
#   generateTests.py                    write conversions.txt and conversionsSI.txt
#   generateTests.py --check            compare computed values against the existing files
#   generateTests.py --stamp <file>     write both files, then touch <file> (meson)
#
# Sources parsed (regexes anchored to the current source formats):
#   src/generateConstants/generateConstants.c  generateConstant("Name", digits, X, "+value")
#   src/c47/defines.h                          #define values for #if selection of constants
#   src/c47/conversionUnits.h                  constFactorXxx enum order
#   src/c47/conversionUnits.c                  conversionFactors[slot], cvtTempConsts[][4], convertPairs[]
#   src/c47/items.c                            UNIT_CONV(factor, ops) and raw fnCvt* items
#   src/c47/items.h                            ITM_ name of each item number
#
# Formula per family, mirroring conversionUnits.c (input is always 1):
#   fnUnitConvert   unitConversion(): optional 1/x, then x*c or x/c        (c.c:854)
#   fnCvtTemp       ((x - B) / C) * D + E, row ix of cvtTempConsts         (c.c:1089)
#   fnCvtDeg/Grad/Rad  unitConversion(180onPi | 9on10 | 200onPi, mul/div)  (c.c:1119-1141)
#   fuel fns        factor built from constants, then 1/x * factor        (c.c:1143-1187)
#   fnCvtHMSHR      sexagesimal encode/decode                              (c.c:1190)
#   fnCvtRatioDb    (10 or 20) * log10(x)                                  (c.c:1212)
#   fnCvtDbRatio    10 ^ (x / (10 or 20))                                  (c.c:1240)
#   runConversionToSI    run the unity item, then scale by 10^exp          (c.c:757)
#   runConversionFromSI  scale by 10^-exp, run the unity item's partner,   (c.c:770)
#                        then run the item's own partner
#
# Maintenance, adding conversions:
#   - Plain factor pair (new UNIT_CONV lines + convertPairs[] rows + constant): nothing to do here.
#     The item set comes from convertPairs[], the factor and direction from the UNIT_CONV line,
#     the constant from conversionFactors[] and generateConstants.c. Automatic.
#
#   - New item using an existing formula function (fnCvtTemp etc.): also automatic, provided the
#     function name is in FNS below; load_items() only matches item lines naming an FNS function.
#     New formula function: 
#     1) add the name to FNS, 
#     2) add a branch in predict() mirroring the C arithmetic step by step in CTX39 with a comment citing the C lines, ending in round34().
#     Until both are done, generation aborts naming the item and function.
#
import os
import re
import sys
from decimal import Decimal, Context, ROUND_HALF_EVEN

HERE      = os.path.dirname(os.path.abspath(__file__))
SRC       = os.path.normpath(os.path.join(HERE, '..'))
OUTPUT    = f'{SRC}/testSuite/tests/conversions.txt'
OUTPUT_SI = f'{SRC}/testSuite/tests/conversionsSI.txt'

PASSES = 2                                                  # identical; catches state contamination

# Every conversion function reachable from convertPairs[] items, except fnUnitConvert which has
# its own UNIT_CONV macro shape. A new formula function must be added here AND given a branch
# in predict(), see the maintenance note in the header.
FNS = ('fnCvtTemp|fnCvtDbRatio|fnCvtRatioDb|fnCvtHMSHR|fnCvtDegRad|fnCvtDegGrad|'
       'fnCvtGradRad|fnKmletok100K|fnL100Tomgus|fnL100Tomguk|fnMgeustok100M|fnMgeuktok100M')

CTX39 = Context(prec=39, rounding=ROUND_HALF_EVEN)           # ctxtReal39, the C working precision
CTX34 = Context(prec=34, rounding=ROUND_HALF_EVEN)           # real34, the result register

#**************************************************************************************************
#* round34
#* Purpose:     final rounding of a computed value to the result register width
#* Source:      a Decimal at working precision
#* Destination: Decimal rounded to 34 significant digits half even, as real34 stores it
#* Function:    CTX34.plus() applies the 34-digit context rounding and nothing else
#**************************************************************************************************
def round34(x):
  return CTX34.plus(x)

#**************************************************************************************************
#* load_constants
#* Purpose:     the value of every generated constant, by name
#* Source:      src/generateConstants/generateConstants.c; src/c47/defines.h for #if selection
#* Destination: dict, e.g. {'MiToKm': Decimal('1.609344'), ...}
#* Function:    matches generateConstant("MiToKm", 7, EXACT, "+1.609344") lines and stores the
#*              quoted value at 39 digits. Some constants are defined twice under
#*              #if (NAME == 1) / #else (e.g. MMHG_PA_133_3224 selects BS 350 abbreviated vs
#*              non-abbreviated mmHg/inHg): the #define values are read from defines.h and the
#*              inactive branch is skipped exactly as the compiler skips it.
#**************************************************************************************************
def load_constants():
  defines = {}
  for line in open(f'{SRC}/c47/defines.h', encoding='utf-8'):
    m = re.match(r'#define\s+(\w+)\s+(\d+)', line)
    if m:
      defines[m.group(1)] = int(m.group(2))
  consts = {}
  active = [True]                                            # one bool per nested #if level
  for line in open(f'{SRC}/generateConstants/generateConstants.c', encoding='utf-8'):
    if re.match(r'\s*#if', line):                            # any #if/#ifdef: push a level
      m = re.match(r'\s*#if\s*\((\w+)\s*==\s*(\d+)\)', line)
      # only the (NAME == n) form is evaluated; other #if forms guard no constants, so True
      active.append(defines.get(m.group(1)) == int(m.group(2)) if m else True)
      continue
    if re.match(r'\s*#else', line):
      active[-1] = not active[-1]
      continue
    if re.match(r'\s*#endif', line):
      active.pop()
      continue
    if not all(active):
      continue
    m = re.search(r'generateConstant\("([^"]+)",\s*\d+,\s*\w+,\s*"([+-][0-9.eE+-]+)"', line)
    if m:
      consts[m.group(1)] = CTX39.plus(Decimal(m.group(2)))   # stored at 39 digits max
  return consts

#**************************************************************************************************
#* const_by_cname
#* Purpose:     resolve a C constant identifier to its value
#* Source:      the dict from load_constants()
#* Destination: Decimal value of the constant
#* Function:    strips the const_ / const39_ / const51_ prefix and looks the name up;
#*              const_1e_NN power-of-ten constants are formed directly (1e-NN); aborts
#*              naming the identifier when it does not exist in generateConstants.c
#**************************************************************************************************
def const_by_cname(consts, cname):
  name = re.sub(r'^const(39|51)?_', '', cname)
  if name in consts:
    return consts[name]
  if re.match(r'^1e_\d+$', name):                            # const_1e_12 = 1e-12
    return Decimal(f'1e-{name[3:]}')
  sys.exit(f'constant {cname} not found in generateConstants.c.\n'
           f'  Fix: add a generateConstant("{name}", ...) line there (then refresh the stale '
           f'generated header: install -C build.sim/src/generateConstants/'
           f'constantPointers{{.h,.c,2.c}} src/generated/), or correct the identifier in '
           f'conversionFactors[] / cvtTempConsts[] in src/c47/conversionUnits.c.')

#**************************************************************************************************
#* load_factor_slots
#* Purpose:     which constant each conversion factor slot points to
#* Source:      src/c47/conversionUnits.h (enum) and src/c47/conversionUnits.c (initialisers)
#* Destination: dict, e.g. {'constFactorInhgPa': 'const_InhgToPa', ...}
#* Function:    two searches: enum members one per line in the .h
#*                  constFactorFt2Hectare,   /* 0 */
#*              and designated initialisers in the .c
#*                  [constFactorInhgPa] = const_InhgToPa,
#*              every enum member must have an initialiser; aborts listing any that do not
#**************************************************************************************************
def load_factor_slots():
  order = []
  for line in open(f'{SRC}/c47/conversionUnits.h', encoding='utf-8'):
    m = re.match(r'\s*(constFactor\w+)\s*[,=]', line)
    if m and m.group(1) != 'constFactorEND':
      order.append(m.group(1))
  slot_const = {}
  for line in open(f'{SRC}/c47/conversionUnits.c', encoding='utf-8'):
    m = re.match(r'\s*\[(constFactor\w+)\]\s*=\s*(\w+)', line)
    if m:
      slot_const[m.group(1)] = m.group(2)
  missing = [s for s in order if s not in slot_const]
  if missing:
    sys.exit(f'conversionFactors[] has no entry for: {missing}.\n'
             f'  Fix: every constFactor enum member (src/c47/conversionUnits.h) needs a '
             f'designated initialiser [slot] = const_Name in conversionFactors[] '
             f'(src/c47/conversionUnits.c); add the missing line(s) there.')
  return slot_const

#**************************************************************************************************
#* load_temp_rows
#* Purpose:     the temperature conversion coefficients B, C, D, E per fnCvtTemp index
#* Source:      src/c47/conversionUnits.c, the cvtTempConsts[][4] table
#* Destination: list of 4-tuples of constant names, in declaration order
#* Function:    collects every row between the cvtTempConsts declaration and its closing brace:
#*                  {const_0, const_1, const_9on5, const_32}, // ITM_CtoF ix = 0
#*              row position = the ix parameter of the fnCvtTemp item in items.c; order matters
#**************************************************************************************************
def load_temp_rows():
  rows, active = [], False
  for line in open(f'{SRC}/c47/conversionUnits.c', encoding='utf-8'):
    if 'cvtTempConsts' in line and '[4]' in line:
      active = True
      continue
    if active:
      m = re.match(r'\s*\{(\w+),\s*(\w+),\s*(\w+),\s*(\w+)\s*\}', line)
      if m:
        rows.append(m.groups())
      elif '};' in line:
        break
  if not rows:
    sys.exit('cvtTempConsts[][4] not found in src/c47/conversionUnits.c.\n'
             '  Fix: the table was renamed or reformatted; align the load_temp_rows() search '
             'in this script with the new declaration.')
  return rows

#**************************************************************************************************
#* load_convert_pairs
#* Purpose:     THE table under test: every convertPairs[] row with all its columns
#* Source:      src/c47/conversionUnits.c, the convertPairs[] table
#* Destination: list of dicts in table order, one per row:
#*                  {'item': 220, 'partner': 221, 'unity': 2669, 'exp': 0, 'ut': 'UT_TEMPERATURE'}
#* Function:    parses every row between the declaration and its closing brace:
#*                  { ITM_CtoF /* 220 */, ITM_FtoC, ITM_FtoK, +0, UT_TEMPERATURE },
#*              partner and unity names resolve to item numbers via items.h (ITM_NULL -> 0);
#*              an unresolvable name aborts telling which row and column to fix. A new pair is
#*              picked up automatically once its convertPairs[] row exists.
#**************************************************************************************************
def load_convert_pairs(numbers_by_name):
  rows, active = [], False
  pat = re.compile(r'\s*\{\s*(ITM_\w+)\s*/\*\s*(\d+)\s*\*/\s*,\s*(\w+)\s*,\s*(\w+)\s*,'
                   r'\s*([+-]?\d+)\s*,\s*(UT_\w+)')
  for line in open(f'{SRC}/c47/conversionUnits.c', encoding='utf-8'):
    if 'convertPairs[NUM_CONVERT_PAIRS]' in line:
      active = True
      continue
    if active:
      m = pat.match(line)
      if m:
        row = {'item': int(m.group(2)), 'exp': int(m.group(5)), 'ut': m.group(6)}
        for column, name in (('partner', m.group(3)), ('unity', m.group(4))):
          if name == 'ITM_NULL':
            row[column] = 0
          elif name in numbers_by_name:
            row[column] = numbers_by_name[name]
          else:
            sys.exit(f'convertPairs row of {m.group(1)}: {column} {name} has no define in '
                     f'items.h.\n  Fix: correct the {column} column of that row in '
                     f'src/c47/conversionUnits.c, or add the missing item define.')
        rows.append(row)
      elif '};' in line:
        break
  return rows

#**************************************************************************************************
#* left_unit_of
#* Purpose:     the source unit's display tokens, for the unit consistency check
#* Source:      the catalog name field of an item line, e.g.  "mph"  STD_RIGHT_ARROW
#* Destination: normalised token string of everything before the arrow, e.g. '"mph"' or
#*              '"ft" STD_SUP_2'; two items converting FROM the same unit yield equal strings
#* Function:    strips /* comments */, cuts at STD_RIGHT_ARROW, collapses whitespace
#**************************************************************************************************
def left_unit_of(cat_field):
  cat = re.sub(r'/\*.*?\*/', '', cat_field)
  cat = cat.split('STD_RIGHT_ARROW')[0]
  return ' '.join(cat.split())

#**************************************************************************************************
#* load_items
#* Purpose:     each conversion item's function, argument and source unit name
#* Source:      src/c47/items.c item table lines
#* Destination: dict item number -> (function name, argument, left unit tokens)
#* Function:    matches the two line shapes that exist:
#*              UNIT_CONV macro, the multiplicative conversions:
#*                  /* 2860 */  UNIT_CONV(constFactorMphKnot, multiply, "mph" STD_RIGHT_ARROW, ...)
#*                  -> ('fnUnitConvert', ('constFactorMphKnot', 'multiply'), '"mph"')
#*              raw table entry naming a formula function from FNS, first field after the brace:
#*                  /*  220 */  { fnCvtTemp, 0, "..." ... }  -> ('fnCvtTemp', '0', ...)
#*                  /*  367 */  { fnCvtHMSHR, divide, ... }  -> ('fnCvtHMSHR', 'divide', ...)
#*              the leading /* item number */, function, first argument and the catalog name
#*              field (up to the next comma) are read; the rest of the line is display data.
#*              An item using a function absent from FNS is not matched at all, and
#*              predict_all() aborts on it by name.
#**************************************************************************************************
def load_items():
  items = {}
  pat_unit = re.compile(r'/\*\s*(\d+)\s*\*/\s*UNIT_CONV\((\w+)\s*,\s*([\w| ]+?)\s*,(.*)$')
  pat_raw  = re.compile(r'/\*\s*(\d+)\s*\*/\s*\{\s*(' + FNS + r')\s*,\s*([\w| ]+?)\s*,(.*)$')
  for line in open(f'{SRC}/c47/items.c', encoding='utf-8'):
    m = pat_unit.match(line)
    if m:
      left = left_unit_of(m.group(4).split(',')[0])
      items[int(m.group(1))] = ('fnUnitConvert', (m.group(2), m.group(3)), left)
      continue
    m = pat_raw.match(line)
    if m:
      left = left_unit_of(m.group(4).split(',')[0])
      items[int(m.group(1))] = (m.group(2), m.group(3).strip(), left)
  return items

#**************************************************************************************************
#* load_item_names
#* Purpose:     item number <-> ITM_ name, for Item: lines, test labels and convertPairs columns
#* Source:      src/c47/items.h defines
#* Destination: two dicts: number -> name {2860: 'ITM_MPHtoKNOT'} and name -> number
#* Function:    matches   #define ITM_MPHtoKNOT               2860
#*              first define wins where aliases share a number; names are unique
#**************************************************************************************************
def load_item_names():
  by_number, by_name = {}, {}
  for line in open(f'{SRC}/c47/items.h', encoding='utf-8'):
    m = re.match(r'#define\s+(ITM_\w+)\s+(\d+)\s*$', line)
    if m:
      by_number.setdefault(int(m.group(2)), m.group(1))
      by_name[m.group(1)] = int(m.group(2))
  return by_number, by_name

#**************************************************************************************************
#* unit_conversion
#* Purpose:     the shared multiplicative conversion arithmetic
#* Source:      input value, coefficient from const_by_cname(), ops string from the item line
#* Destination: Decimal result as the calculator computes it
#* Function:    mirrors unitConversion() at conversionUnits.c:854: optional 1/x when ops
#*              contains 'invert', then x*coeff or x/coeff ('divide'), every operation in the
#*              39-digit context, result rounded to 34
#**************************************************************************************************
def unit_conversion(x, coeff, ops):
  if 'invert' in ops:
    x = CTX39.divide(Decimal(1), x)
  if 'divide' in ops:
    x = CTX39.divide(x, coeff)
  else:
    x = CTX39.multiply(x, coeff)
  return round34(x)

#**************************************************************************************************
#* temp_const
#* Purpose:     resolve one cvtTempConsts coefficient
#* Source:      a constant name from a load_temp_rows() row
#* Destination: Decimal value
#* Function:    const_0 / const_1 are skip markers in the C, numerically plain 0 and 1;
#*              everything else resolves through const_by_cname()
#**************************************************************************************************
def temp_const(consts, name):
  if name == 'const_0':
    return Decimal(0)
  if name == 'const_1':
    return Decimal(1)
  return const_by_cname(consts, name)

#**************************************************************************************************
#* predict
#* Purpose:     compute the expected result of one conversion item for input x
#* Source:      the item's (function, argument) from load_items(), the loaded tables in D,
#*              and the input value x (1 for the direct tests; chained values for SI tests)
#* Destination: Decimal result, rounded to 34 digits
#* Function:    one branch per formula family, each mirroring its C function operation by
#*              operation: same operand order, every step in CTX39, finish with round34().
#*              When adding a branch, cite the C lines it mirrors and change nothing about
#*              the arithmetic shape. Aborts naming any function without a branch.
#**************************************************************************************************
def predict(D, fn, arg, x):
  consts = D['consts']
  if fn == 'fnUnitConvert':
    factor, ops = arg
    coeff = const_by_cname(consts, D['slots'][factor])
    return unit_conversion(x, coeff, ops)
  if fn == 'fnCvtTemp':                                      # ((x - B) / C) * D + E
    B, C, Dd, E = (temp_const(consts, n) for n in D['temp_rows'][int(arg)])
    x = CTX39.subtract(x, B)
    x = CTX39.divide(x, C)
    x = CTX39.multiply(x, Dd)
    x = CTX39.add(x, E)
    return round34(x)
  if fn == 'fnCvtDegRad':
    return unit_conversion(x, const_by_cname(consts, 'const39_180onPi'), arg)
  if fn == 'fnCvtDegGrad':
    return unit_conversion(x, const_by_cname(consts, 'const_9on10'), arg)
  if fn == 'fnCvtGradRad':
    return unit_conversion(x, const_by_cname(consts, 'const39_200onPi'), arg)
  if fn in ('fnKmletok100K', 'fnMgeustok100M'):              # 100 * GaluseqE [/ GalusToL]
    f = CTX39.multiply(consts['GaluseqE'], Decimal(100))
    if fn == 'fnKmletok100K':
      f = CTX39.divide(f, consts['GalusToL'])
    return unit_conversion(x, f, 'invert multiply')
  if fn in ('fnL100Tomgus', 'fnL100Tomguk'):                 # 100 * gallon / MiToKm
    gal = consts['GalusToL'] if fn == 'fnL100Tomgus' else consts['GalukToL']
    f = CTX39.multiply(Decimal(100), gal)
    f = CTX39.divide(f, consts['MiToKm'])
    return unit_conversion(x, f, 'invert multiply')
  if fn == 'fnMgeuktok100M':                                 # 100 * GaluseqE * GalukToL / GalusToL
    f = CTX39.multiply(consts['GaluseqE'], Decimal(100))
    f = CTX39.multiply(f, consts['GalukToL'])
    f = CTX39.divide(f, consts['GalusToL'])
    return unit_conversion(x, f, 'invert multiply')
  if fn == 'fnCvtHMSHR':                                     # sexagesimal
    if arg == 'divide':                                      # HMS -> HR: H.MMSS to decimal hours
      h = int(x)
      mmss = CTX39.multiply(x - h, Decimal(100))
      m = int(mmss)
      s = CTX39.multiply(mmss - m, Decimal(100))
      hr = CTX39.add(Decimal(h), CTX39.divide(Decimal(m), Decimal(60)))
      return round34(CTX39.add(hr, CTX39.divide(s, Decimal(3600))))
    h = int(x)                                               # HR -> HMS: decimal hours to H.MMSS
    rem = CTX39.multiply(x - h, Decimal(60))
    m = int(rem)
    s = CTX39.multiply(rem - m, Decimal(60))
    hms = CTX39.add(Decimal(h), CTX39.divide(Decimal(m), Decimal(100)))
    return round34(CTX39.add(hms, CTX39.divide(s, Decimal(10000))))
  if fn == 'fnCvtRatioDb':                                   # (10|20) * log10(x)
    return round34(CTX39.multiply(CTX39.log10(x), Decimal(arg)))
  if fn == 'fnCvtDbRatio':                                   # 10 ^ (x / (10|20))
    return round34(CTX39.power(Decimal(10), CTX39.divide(x, Decimal(arg))))
  sys.exit(f'no predictor for {fn}: add a branch above mirroring the C, see the header note')

#**************************************************************************************************
#* load_all
#* Purpose:     one load of every parsed source, shared by generation and checks
#* Source:      all load_* functions
#* Destination: dict D with keys consts, slots, temp_rows, items, names (number -> ITM_ name),
#*              rows (convertPairs), byitem (item number -> its convertPairs row)
#* Function:    plain aggregation; aborts inside the loaders on malformed sources
#**************************************************************************************************
def load_all():
  names, numbers = load_item_names()
  rows = load_convert_pairs(numbers)
  return {
    'consts':    load_constants(),
    'slots':     load_factor_slots(),
    'temp_rows': load_temp_rows(),
    'items':     load_items(),
    'names':     names,
    'rows':      rows,
    'byitem':    {row['item']: row for row in rows},
  }

#**************************************************************************************************
#* item_fn_arg
#* Purpose:     an item's (function, argument) with the standard abort text
#* Source:      D['items'] and D['names']
#* Destination: (fn, arg) tuple
#* Function:    aborts naming the item when it is not matched in items.c: either the item
#*              number in the convertPairs row is wrong, or the item uses a new formula
#*              function that must be added to FNS and predict()
#**************************************************************************************************
def item_fn_arg(D, item):
  if item not in D['items']:
    sys.exit(f'convertPairs item {item} ({D["names"].get(item, "?")}) not matched in items.c.\n'
             f'  Fix: if the item number is right, its table line names a formula function '
             f'missing from FNS; add the name to FNS and a branch to predict().')
  fn, arg, left = D['items'][item]
  return fn, arg

#**************************************************************************************************
#* predict_all
#* Purpose:     the complete direct-conversion test set
#* Source:      D from load_all()
#* Destination: list of (label, item number, ITM_ name, value) in convertPairs order,
#*              e.g. ('T220_CtoF', 220, 'ITM_CtoF', Decimal('33.8'))
#* Function:    runs predict() with input 1 for every convertPairs[] item; aborts when an
#*              item has no ITM_ define in items.h
#**************************************************************************************************
def predict_all(D):
  result = []
  for row in D['rows']:
    item = row['item']
    if item not in D['names']:
      sys.exit(f'convertPairs item {item} has no ITM_ define in items.h.\n'
               f'  Fix: the row in src/c47/conversionUnits.c names an item that does not '
               f'exist; correct the item, or add the define to items.h.')
    fn, arg = item_fn_arg(D, item)
    value = predict(D, fn, arg, Decimal(1))
    result.append((f'T{item}_{D["names"][item][4:]}', item, D['names'][item], value))
  return result

#**************************************************************************************************
#* predict_to_si
#* Purpose:     the expected result of the ToSI round-trip half for input 1
#* Source:      one convertPairs row and D
#* Destination: Decimal result, rounded to 34 digits
#* Function:    mirrors runConversionToSI (conversionUnits.c:757): run the unity item's
#*              conversion, then scale by 10^exp (fnMultiplySI, an exact digit shift).
#*              Chained values pass through the register between steps, hence round34 each step.
#**************************************************************************************************
def predict_to_si(D, row):
  x = Decimal(1)
  if row['unity'] != 0:
    fn, arg = item_fn_arg(D, row['unity'])
    x = predict(D, fn, arg, x)
  if row['exp'] != 0:
    x = round34(x.scaleb(row['exp'], CTX39))
  return x

#**************************************************************************************************
#* predict_from_si
#* Purpose:     the expected result of the FromSI round-trip half for input 1
#* Source:      one convertPairs row and D
#* Destination: Decimal result, rounded to 34 digits
#* Function:    mirrors runConversionFromSI (conversionUnits.c:770): scale by 10^-exp, run the
#*              unity item's PARTNER, then run the item's own partner. Chained values pass
#*              through the register between steps, hence round34 after each conversion.
#**************************************************************************************************
def predict_from_si(D, row):
  x = Decimal(1)
  if row['exp'] != 0:
    x = round34(x.scaleb(-row['exp'], CTX39))
  if row['unity'] != 0:
    unity_partner = D['byitem'][row['unity']]['partner']
    fn, arg = item_fn_arg(D, unity_partner)
    x = predict(D, fn, arg, x)
  fn, arg = item_fn_arg(D, row['partner'])
  return predict(D, fn, arg, x)

#**************************************************************************************************
#* predict_si_all
#* Purpose:     the complete SI round-trip test set
#* Source:      D from load_all()
#* Destination: list of (label, wrapper function, item number, value); two entries per
#*              configurable item, e.g. ('T220_CtoF_toSI', 'covConvToSI', 220, ...)
#* Function:    UT_NOT_CONFIGURABLE items are skipped: their unity is never called, custom
#*              pairs cannot be formed on them
#**************************************************************************************************
def predict_si_all(D):
  result = []
  for row in D['rows']:
    if row['ut'] == 'UT_NOT_CONFIGURABLE':
      continue
    stem = f'T{row["item"]}_{D["names"][row["item"]][4:]}'
    result.append((f'{stem}_toSI',   'covConvToSI',   row['item'], predict_to_si(D, row)))
    result.append((f'{stem}_fromSI', 'covConvFromSI', row['item'], predict_from_si(D, row)))
  return result

#**************************************************************************************************
#* check_pairs_table
#* Purpose:     abort the build when convertPairs[] is wired in a way the C cannot execute
#* Source:      D from load_all()
#* Destination: prints every problem WITH its fix, then aborts; silent when the table is sound
#* Function:    per row: the partner must be a convertPairs item and point back (FromSI runs
#*              it as the inverse); a configurable item's unity must be a convertPairs item of
#*              the same UT type; the unity's own unity must be ITM_NULL, because
#*              runConversionToSI executes exactly ONE hop (conversionUnits.c:757), so a
#*              two-hop chain would silently produce non-SI values; UT_NOT_CONFIGURABLE rows
#*              must have unity ITM_NULL
#**************************************************************************************************
def check_pairs_table(D):
  problems = []
  for row in D['rows']:
    item, name = row['item'], D['names'].get(row['item'], '?')
    where = f'convertPairs row of item {item} ({name})'
    partner = D['byitem'].get(row['partner'])
    if partner is None:
      problems.append(f'{where}: partner {row["partner"]} is not a convertPairs[] item.\n'
                      f'  Fix: every pair needs BOTH rows in convertPairs[] '
                      f'(src/c47/conversionUnits.c); add the reverse row or correct the '
                      f'partner column.')
    elif partner['partner'] != item:
      problems.append(f'{where}: partner is {row["partner"]}, but that row\'s partner is '
                      f'{partner["partner"]}, not {item}.\n'
                      f'  Fix: the two rows of a pair must point at each other in the partner '
                      f'column.')
    if row['ut'] == 'UT_NOT_CONFIGURABLE':
      if row['unity'] != 0:
        problems.append(f'{where}: type UT_NOT_CONFIGURABLE but unity is {row["unity"]}; '
                        f'unity is never called for this type.\n'
                        f'  Fix: set the unity column to ITM_NULL.')
      continue
    if row['unity'] == 0:
      continue                                               # output IS the SI base
    unity = D['byitem'].get(row['unity'])
    if unity is None:
      problems.append(f'{where}: unity {row["unity"]} is not a convertPairs[] item.\n'
                      f'  Fix: set the unity column to the item that converts {name}\'s '
                      f'OUTPUT unit one hop to the SI base of {row["ut"]}, or ITM_NULL if '
                      f'the output already IS the SI base.')
      continue
    if unity['ut'] != row['ut']:
      problems.append(f'{where}: unity item {row["unity"]} has type {unity["ut"]}, the item '
                      f'is {row["ut"]}.\n'
                      f'  Fix: unity must be a conversion of the same physical type: the '
                      f'{row["ut"]} item whose input is {name}\'s output unit and whose '
                      f'output is the SI base.')
    if unity['unity'] != 0:
      problems.append(f'{where}: unity item {row["unity"]} itself has unity '
                      f'{unity["unity"]}, i.e. its output is NOT the SI base, but '
                      f'runConversionToSI executes exactly ONE hop (conversionUnits.c:757), '
                      f'so custom pairs on {name} would never reach SI.\n'
                      f'  Fix: point the unity column at an item converting straight to the '
                      f'SI base (an item whose own unity is ITM_NULL); if none exists, add '
                      f'that direct conversion pair first.')
  if problems:
    sys.exit('convertPairs[] table check FAILED - the table is in src/c47/conversionUnits.c, '
             'all rows/columns below refer to it:\n' + '\n'.join(problems))

#**************************************************************************************************
#* check_unit_consistency
#* Purpose:     catch wrong factors, polarity, unity or exponent through name redundancy
#* Source:      D from load_all()
#* Destination: prints every disagreeing group WITH the values and what to check, then aborts
#* Function:    items whose catalog name starts with the same source unit (e.g. the mph->knot
#*              and mph->ft/s items) must reach the SAME SI value for 1 unit via their own
#*              route (own conversion, then unity, then exponent). The display name is ground
#*              truth the factor table does not know, so this is an independent cross-check.
#*              UT_FUELECON and UT_EVECON are excluded: those units are reciprocal, so items
#*              sharing a source unit legitimately reach different values (1 l/100km is both
#*              100 km/l and 235.2 mpg US). Tolerance is 1e-4 relative: novelty units
#*              (furlong/fortnight, barleycorn, banana, firkin) use short constants and their
#*              routes legitimately disagree by up to ~2.5e-6, while wiring bugs (swapped
#*              multiply/divide, wrong constant, wrong exponent) are orders of magnitude out.
#**************************************************************************************************
def check_unit_consistency(D):
  reciprocal_types = ('UT_FUELECON', 'UT_EVECON')
  groups = {}
  for row in D['rows']:
    if row['ut'] == 'UT_NOT_CONFIGURABLE' or row['ut'] in reciprocal_types:
      continue
    fn, arg = item_fn_arg(D, row['item'])
    left = D['items'][row['item']][2]
    x = predict(D, fn, arg, Decimal(1))                      # 1 source unit -> output unit
    if row['unity'] != 0:
      ufn, uarg = item_fn_arg(D, row['unity'])
      x = predict(D, ufn, uarg, x)                           # output unit -> SI base
    if row['exp'] != 0:
      x = round34(x.scaleb(row['exp'], CTX39))
    groups.setdefault((row['ut'], left), []).append((row['item'], x))
  problems = []
  for (ut, left), members in groups.items():
    if len(members) < 2:
      continue
    reference = members[0][1]
    for item, value in members[1:]:
      if abs(value - reference) > abs(reference) * Decimal('1e-4'):
        listing = ', '.join(f'item {i} ({D["names"][i]}) -> {v}' for i, v in members)
        problems.append(f'unit consistency ({ut}): these items all convert FROM {left} but '
                        f'reach different SI values for 1 unit: {listing}.\n'
                        f'  Fix: one of them is wrong. Recompute 1 {left} in SI by hand, '
                        f'then check each item\'s UNIT_CONV factor and multiply/divide '
                        f'(items.c), its unity and exp columns (convertPairs[]), and the '
                        f'constant it uses (generateConstants.c).')
        break
  if problems:
    sys.exit('unit consistency check FAILED - factors/polarity live in src/c47/items.c, '
             'unity/exp in convertPairs[] in src/c47/conversionUnits.c, constant values in '
             'src/generateConstants/generateConstants.c:\n' + '\n'.join(problems))

#**************************************************************************************************
#* render
#* Purpose:     format the test set as a testSuite test file
#* Source:      the list from predict_all()
#* Destination: the full text of conversions.txt (returned as one string)
#* Function:    identity header, one In: line of suite defaults, then per pass and item the
#*              4-line block: label comment, Item: name, In: RX=1, Out: expected value
#**************************************************************************************************
def render(tests):
  out = []
  out.append(';Do not edit this file manually! It is automagically generated by generateTests.py')
  out.append(';(src/generateTests/), which computes every value below from generateConstants.c and')
  out.append(';the item tables, independently of the calculator code. The constants themselves are')
  out.append(';not verified here: test and calculator share the same source. Every convertPairs[]')
  out.append(';item converts input 1 via the real dispatch chain (Item: -> reallyRunFunction);')
  out.append(f';register X must match bit-exact. {PASSES} identical passes catch state contamination.')
  out.append('')
  out.append('In: FL_SPCRES=0 FL_CPXRES=0 SD=0 RMODE=0')
  for p in range(PASSES):                                    # the identical passes: the same
    out.append('')                                           # item blocks repeated PASSES times
    out.append(f';============================ pass {p + 1} ============================')
    for label, item, name, value in tests:
      out.append('')
      out.append(f'; {label}')
      out.append(f'Item: {name}')
      out.append('In:  RX=Real:"1"')
      out.append(f'Out: EC=0 RX=Real:"{value}"')
  return '\n'.join(out) + '\n'

#**************************************************************************************************
#* render_si
#* Purpose:     format the SI round-trip test set as a testSuite test file
#* Source:      the list from predict_si_all()
#* Destination: the full text of conversionsSI.txt (returned as one string)
#* Function:    identity header, one In: line of suite defaults, then per pass and half the
#*              4-line block: label comment, Func: wrapper(item), In: RX=1, Out: expected value
#**************************************************************************************************
def render_si(tests):
  out = []
  out.append(';Do not edit this file manually! It is automagically generated by generateTests.py')
  out.append(';(src/generateTests/), which computes every value below from generateConstants.c and')
  out.append(';the item tables, independently of the calculator code. The constants themselves are')
  out.append(';not verified here: test and calculator share the same source. Per configurable')
  out.append(';convertPairs[] item both SI round-trip halves run: covConvToSI = runConversionToSI')
  out.append(';(the unity conversion then 10^exp) and covConvFromSI = runConversionFromSI (10^-exp,')
  out.append(';the unity partner, the item partner) - the machinery behind custom conversion pairs.')
  out.append(f';Register X must match bit-exact. {PASSES} identical passes catch state contamination.')
  out.append('')
  out.append('In: FL_SPCRES=0 FL_CPXRES=0 SD=0 RMODE=0')
  for p in range(PASSES):                                    # the identical passes: the same
    out.append('')                                           # blocks repeated PASSES times
    out.append(f';============================ pass {p + 1} ============================')
    for label, wrapper, item, value in tests:
      out.append('')
      out.append(f'; {label}')
      out.append(f'Func: {wrapper}({item})')
      out.append('In:  RX=Real:"1"')
      out.append(f'Out: EC=0 RX=Real:"{value}"')
  return '\n'.join(out) + '\n'

#**************************************************************************************************
#* check
#* Purpose:     --check mode: compare computed values against an existing file, write nothing
#* Source:      expected (label, value) pairs and the file to compare against
#* Destination: one line per problem (MISMATCH / MISSING / EXTRA, by label) plus a summary
#*              count on stdout; returns the problem count
#* Function:    reads the stored value under each '; T...' label comment, compares numerically
#*              per label; EXTRA flags stored labels no longer generated
#**************************************************************************************************
def check(expected, path):
  stored, label = {}, None
  for line in open(path, encoding='utf-8'):
    m = re.match(r'; (T\w+)', line)
    if m:
      label = m.group(1)
    m = re.match(r'Out: EC=0 RX=Real:"([^"]+)"', line)
    if m and label is not None:
      stored.setdefault(label, Decimal(m.group(1)))
      label = None
  failed = 0
  for label, value in expected:
    if label not in stored:
      print(f'MISSING  {label} not in {path}')
      failed += 1
    elif value.compare(stored[label]) != 0:
      print(f'MISMATCH {label}: computed {value}, stored {stored[label]}')
      failed += 1
  for label in sorted(set(stored) - {label for label, _ in expected}):
    print(f'EXTRA    {label} in {path} but no longer generated')
    failed += 1
  print(f'{len(expected)} values checked against {os.path.basename(path)}, {failed} problem(s)')
  return failed

#**************************************************************************************************
#* main
#* Purpose:     command line entry, see Usage in the header
#* Source:      sys.argv
#* Destination: conversions.txt and conversionsSI.txt (default and --stamp, which also touches
#*              the meson stamp file) or the --check report; exit 0 = OK, 1 = check problems
#* Function:    always runs the table and unit consistency checks first (they abort with fix
#*              instructions); then no arguments and --stamp generate, --check only compares
#**************************************************************************************************
def main():
  args = sys.argv[1:]
  D = load_all()
  check_pairs_table(D)
  check_unit_consistency(D)
  tests    = predict_all(D)
  tests_si = predict_si_all(D)
  if args == ['--check']:
    failed  = check([(label, value) for label, item, name, value in tests], OUTPUT)
    failed += check([(label, value) for label, wrapper, item, value in tests_si], OUTPUT_SI)
    return 1 if failed else 0
  if args == [] or (len(args) == 2 and args[0] == '--stamp'):
    with open(OUTPUT, 'w', encoding='utf-8') as f:
      f.write(render(tests))
    print(f'{len(tests)} items x {PASSES} passes written to {OUTPUT}')
    with open(OUTPUT_SI, 'w', encoding='utf-8') as f:
      f.write(render_si(tests_si))
    print(f'{len(tests_si)} SI half-trips x {PASSES} passes written to {OUTPUT_SI}')
    if args:
      open(args[1], 'w').close()                             # the meson stamp file
    return 0
  sys.exit('usage: generateTests.py [--check | --stamp <file>]')

if __name__ == '__main__':
  sys.exit(main())
