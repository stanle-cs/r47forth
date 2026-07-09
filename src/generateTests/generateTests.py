#!/usr/bin/env python3
# Generates src/testSuite/tests/conversions.txt: one test per convertPairs[] item, 2 identical passes, every expected value computed here.
# Runs at build time via meson, same hook as generateConstants, so the test set always matches the sources it was built from.
#
# Reads only the C sources; computes each conversion of input 1 with decimal arithmetic at the calculator's precisions (each operation in a
# 39-digit context, result rounded half even to the 34-digit register). Calculator output is never read, so the testSuite checks the calculator
# against an independent computation of the same definitions.
# 
# NOT covered: the constant values themselves. Test and calculator share generateConstants.c, so a wrong literal there passes both.
# Verifying constants would need an independent source. That is not done here.
#
# Usage:
#   generateTests.py                    write conversions.txt
#   generateTests.py --check            compare computed values against the existing file
#   generateTests.py --stamp <file>     write conversions.txt, then touch <file> (meson)
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

HERE   = os.path.dirname(os.path.abspath(__file__))
SRC    = os.path.normpath(os.path.join(HERE, '..'))
OUTPUT = f'{SRC}/testSuite/tests/conversions.txt'

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
  sys.exit(f'constant {cname} not found in generateConstants.c')

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
    sys.exit(f'conversionFactors[] has no entry for: {missing}')
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
  return rows

#**************************************************************************************************
#* load_convert_pairs
#* Purpose:     THE list of items under test
#* Source:      src/c47/conversionUnits.c, the convertPairs[] table
#* Destination: list of item numbers in table order, e.g. [220, 221, 222, ...]
#* Function:    takes the first column of every row between the declaration and its closing brace:
#*                  { ITM_CtoF /* 220 */, ITM_FtoC, ITM_FtoK, +0, UT_TEMPERATURE },   -> 220
#*              the number comes from the /* comment */; a new pair is picked up automatically
#*              once its convertPairs[] row exists
#**************************************************************************************************
def load_convert_pairs():
  pairs, active = [], False
  for line in open(f'{SRC}/c47/conversionUnits.c', encoding='utf-8'):
    if 'convertPairs[NUM_CONVERT_PAIRS]' in line:
      active = True
      continue
    if active:
      m = re.match(r'\s*\{\s*ITM_\w+\s*/\*\s*(\d+)\s*\*/', line)
      if m:
        pairs.append(int(m.group(1)))
      elif '};' in line:
        break
  return pairs

#**************************************************************************************************
#* load_items
#* Purpose:     each conversion item's function and argument
#* Source:      src/c47/items.c item table lines
#* Destination: dict item number -> (function name, argument)
#* Function:    matches the two line shapes that exist:
#*              UNIT_CONV macro, the multiplicative conversions:
#*                  /* 2860 */  UNIT_CONV(constFactorMphKnot, multiply, ...)
#*                  -> ('fnUnitConvert', ('constFactorMphKnot', 'multiply'))
#*              raw table entry naming a formula function from FNS, first field after the brace:
#*                  /*  220 */  { fnCvtTemp, 0, ... }        -> ('fnCvtTemp', '0')
#*                  /*  367 */  { fnCvtHMSHR, divide, ... }  -> ('fnCvtHMSHR', 'divide')
#*              only the leading /* item number */, function and first argument are read; the
#*              rest of the line is display data. An item using a function absent from FNS is
#*              not matched at all, and predict_all() aborts on it by name.
#**************************************************************************************************
def load_items():
  items = {}
  pat_unit = re.compile(r'/\*\s*(\d+)\s*\*/\s*UNIT_CONV\((\w+)\s*,\s*([\w| ]+?)\s*,')
  pat_raw  = re.compile(r'/\*\s*(\d+)\s*\*/\s*\{\s*(' + FNS + r')\s*,\s*([\w| ]+?)\s*,')
  for line in open(f'{SRC}/c47/items.c', encoding='utf-8'):
    m = pat_unit.match(line)
    if m:
      items[int(m.group(1))] = ('fnUnitConvert', (m.group(2), m.group(3)))
      continue
    m = pat_raw.match(line)
    if m:
      items[int(m.group(1))] = (m.group(2), m.group(3).strip())
  return items

#**************************************************************************************************
#* load_item_names
#* Purpose:     the ITM_ name of each item, for the Item: lines and test labels
#* Source:      src/c47/items.h defines
#* Destination: dict item number -> name, e.g. {2860: 'ITM_MPHtoKNOT', ...}
#* Function:    matches   #define ITM_MPHtoKNOT               2860
#*              first define wins where aliases share a number
#**************************************************************************************************
def load_item_names():
  names = {}
  for line in open(f'{SRC}/c47/items.h', encoding='utf-8'):
    m = re.match(r'#define\s+(ITM_\w+)\s+(\d+)\s*$', line)
    if m and int(m.group(2)) not in names:
      names[int(m.group(2))] = m.group(1)
  return names

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
#* Purpose:     compute the expected result of one conversion item for input 1
#* Source:      the item's (function, argument) from load_items(), plus the loaded tables
#* Destination: Decimal result, rounded to 34 digits
#* Function:    one branch per formula family, each mirroring its C function operation by
#*              operation: same operand order, every step in CTX39, finish with round34().
#*              When adding a branch, cite the C lines it mirrors and change nothing about
#*              the arithmetic shape. Aborts naming any function without a branch.
#**************************************************************************************************
def predict(fn, arg, consts, slot_const, temp_rows):
  one = Decimal(1)
  if fn == 'fnUnitConvert':
    factor, ops = arg
    coeff = const_by_cname(consts, slot_const[factor])
    return unit_conversion(one, coeff, ops)
  if fn == 'fnCvtTemp':                                      # ((x - B) / C) * D + E
    B, C, D, E = (temp_const(consts, n) for n in temp_rows[int(arg)])
    x = CTX39.subtract(one, B)
    x = CTX39.divide(x, C)
    x = CTX39.multiply(x, D)
    x = CTX39.add(x, E)
    return round34(x)
  if fn == 'fnCvtDegRad':
    return unit_conversion(one, const_by_cname(consts, 'const39_180onPi'), arg)
  if fn == 'fnCvtDegGrad':
    return unit_conversion(one, const_by_cname(consts, 'const_9on10'), arg)
  if fn == 'fnCvtGradRad':
    return unit_conversion(one, const_by_cname(consts, 'const39_200onPi'), arg)
  if fn in ('fnKmletok100K', 'fnMgeustok100M'):              # 100 * GaluseqE [/ GalusToL]
    f = CTX39.multiply(consts['GaluseqE'], Decimal(100))
    if fn == 'fnKmletok100K':
      f = CTX39.divide(f, consts['GalusToL'])
    return unit_conversion(one, f, 'invert multiply')
  if fn in ('fnL100Tomgus', 'fnL100Tomguk'):                 # 100 * gallon / MiToKm
    gal = consts['GalusToL'] if fn == 'fnL100Tomgus' else consts['GalukToL']
    f = CTX39.multiply(Decimal(100), gal)
    f = CTX39.divide(f, consts['MiToKm'])
    return unit_conversion(one, f, 'invert multiply')
  if fn == 'fnMgeuktok100M':                                 # 100 * GaluseqE * GalukToL / GalusToL
    f = CTX39.multiply(consts['GaluseqE'], Decimal(100))
    f = CTX39.multiply(f, consts['GalukToL'])
    f = CTX39.divide(f, consts['GalusToL'])
    return unit_conversion(one, f, 'invert multiply')
  if fn == 'fnCvtHMSHR':                                     # sexagesimal; 1 -> 1 both ways
    if arg == 'divide':                                      # HMS -> HR: H.MMSS to decimal hours
      h = int(one)
      mmss = (one - h) * 100
      m = int(mmss)
      s = (mmss - m) * 100
      return round34(Decimal(h) + Decimal(m) / 60 + s / 3600)
    h = int(one)                                             # HR -> HMS: decimal hours to H.MMSS
    rem = (one - h) * 60
    m = int(rem)
    s = (rem - m) * 60
    return round34(Decimal(h) + Decimal(m) / 100 + s / 10000)
  if fn == 'fnCvtRatioDb':                                   # (10|20) * log10(x); log10(1) = 0
    x = CTX39.multiply(CTX39.log10(one), Decimal(arg))
    return round34(x)
  if fn == 'fnCvtDbRatio':                                   # 10 ^ (x / (10|20))
    x = CTX39.divide(one, Decimal(arg))
    x = CTX39.power(Decimal(10), x)
    return round34(x)
  sys.exit(f'no predictor for {fn}: add a branch above mirroring the C, see the header note')

#**************************************************************************************************
#* predict_all
#* Purpose:     the complete computed test set
#* Source:      all load_* results
#* Destination: list of (label, item number, ITM_ name, value) in convertPairs order,
#*              e.g. ('T220_CtoF', 220, 'ITM_CtoF', Decimal('33.8'))
#* Function:    runs predict() for every convertPairs[] item; aborts naming the item when it
#*              is not matched in items.c (new formula function: extend FNS and predict())
#*              or has no ITM_ define in items.h
#**************************************************************************************************
def predict_all():
  consts     = load_constants()
  slot_const = load_factor_slots()
  temp_rows  = load_temp_rows()
  items      = load_items()
  names      = load_item_names()
  result = []
  for item in load_convert_pairs():
    if item not in items:
      sys.exit(f'convertPairs item {item} ({names.get(item, "?")}) not matched in items.c: '
               f'if its table line names a new formula function, add it to FNS and predict()')
    if item not in names:
      sys.exit(f'convertPairs item {item} has no ITM_ define in items.h')
    fn, arg = items[item]
    value = predict(fn, arg, consts, slot_const, temp_rows)
    result.append((f'T{item}_{names[item][4:]}', item, names[item], value))
  return result

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
#* check
#* Purpose:     --check mode: compare computed values against the existing file, write nothing
#* Source:      the list from predict_all() and the current conversions.txt
#* Destination: one line per problem (MISMATCH / MISSING / EXTRA, by item and name) plus a
#*              summary count on stdout; returns the problem count
#* Function:    reads the stored value under each '; T<num>_' label, compares numerically per
#*              item; EXTRA flags stored items no longer present in convertPairs[]
#**************************************************************************************************
def check(tests):
  stored, label = {}, None
  for line in open(OUTPUT, encoding='utf-8'):
    m = re.match(r'; (T(\d+)_\w+)', line)
    if m:
      label = int(m.group(2))
    m = re.match(r'Out: EC=0 RX=Real:"([^"]+)"', line)
    if m and label is not None:
      stored.setdefault(label, Decimal(m.group(1)))
      label = None
  failed = 0
  for _, item, name, value in tests:
    if item not in stored:
      print(f'MISSING  item {item} ({name}) not in {OUTPUT}')
      failed += 1
    elif value.compare(stored[item]) != 0:
      print(f'MISMATCH item {item} ({name}): predicted {value}, stored {stored[item]}')
      failed += 1
  extra = set(stored) - {t[1] for t in tests}
  for item in sorted(extra):
    print(f'EXTRA    item {item} in {OUTPUT} but not in convertPairs[]')
    failed += 1
  print(f'{len(tests)} predictions checked, {failed} problem(s)')
  return failed

#**************************************************************************************************
#* main
#* Purpose:     command line entry, see Usage in the header
#* Source:      sys.argv
#* Destination: conversions.txt (default and --stamp, which also touches the meson stamp file)
#*              or the --check report; exit code 0 = OK, 1 = check found problems
#* Function:    no arguments and --stamp generate; --check only compares
#**************************************************************************************************
def main():
  args = sys.argv[1:]
  if args == ['--check']:
    return 1 if check(predict_all()) else 0
  if args == [] or (len(args) == 2 and args[0] == '--stamp'):
    tests = predict_all()
    with open(OUTPUT, 'w', encoding='utf-8') as f:
      f.write(render(tests))
    print(f'{len(tests)} items x {PASSES} passes written to {OUTPUT}')
    if args:
      open(args[1], 'w').close()                             # the meson stamp file
    return 0
  sys.exit('usage: generateTests.py [--check | --stamp <file>]')

if __name__ == '__main__':
  sys.exit(main())
