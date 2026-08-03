testSuite_src = files(
  'testSuite.c',
  'hal/audio.c',
  'hal/gui.c',
  'hal/lcd.c',
  'hal/io.c',
  'hal/print_ir.c')

# CUSTOM_PKG (fork infrastructure, T2-A): when a package patches under
# src/testSuite, root meson.build phase 2 sets custom_pkg_testSuite_src
# to the shadow-tree source list; compile from the shadow so the package
# patch is visible, exactly as c47_src already does. Unset otherwise.
if get_variable('custom_pkg_testSuite_src', []) != []
  testSuite_src = custom_pkg_testSuite_src
endif

testSuite = executable('testSuite',
  build_by_default    : false,
  sources             : decNumber_src + c47_src + testSuite_src + [rasterFontsData_c, softmenuCatalogs_h, constantPointers_ch, conversionsTests_stamp],
  c_args              : '-DTESTSUITE_BUILD',
  include_directories : [decNumber_inc, c47_inc],
  dependencies        : [gtk_dep, gmp_dep, m_dep],
  native              : true
)

# CUSTOM_PKG (fork infrastructure, T6): when a package patches under
# src/testSuite, the case list comes from the shadow tree so patched
# list entries and package-new case files resolve beside it.
if get_variable('custom_pkg_testSuite_list', '') != ''
  testSuite_list_arg = get_variable('custom_pkg_testSuite_list')
else
  testSuite_list_arg = files('tests/testSuiteList.txt')
endif

test(
  'testSuite',
  testSuite,
  args    : [testSuite_list_arg],
  workdir : meson.project_source_root(),
  timeout : 800
)
