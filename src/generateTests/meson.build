py3_generateTests = find_program('python3')

# Regenerates src/testSuite/tests/conversions.txt from first principles on every build that
# needs the testSuite, exactly like generateConstants regenerates constantPointers. Unlike
# the other generators it outputs directly into the testSuite source tree; the stamp file
# below only gives meson a build-directory output to track.
conversionsTests_stamp = custom_target('conversions.txt',
  build_by_default : false,
  input            : files('generateTests.py'),
  output           : 'conversions_txt.stamp',
  command          : [py3_generateTests, '@INPUT@', '--stamp', '@OUTPUT@'],
  depend_files     : files(
    '../generateConstants/generateConstants.c',
    '../c47/defines.h',
    '../c47/conversionUnits.h',
    '../c47/conversionUnits.c',
    '../c47/items.c',
    '../c47/items.h')
)
