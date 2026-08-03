project('c47', 'c',
  meson_version   : '>=0.60.0',
  license         : 'GPL-3.0-only',
  default_options : ['warning_level=2']
)

if get_option('buildtype') == 'custom'
  add_project_arguments('-g3', native: true, language : 'c')
endif

# Simulator and generate tools arguments
add_project_arguments('-DPC_BUILD',   native : true, language : 'c')
if build_machine.system() == 'windows'
  add_project_arguments('-DWIN32',    native : true, language : 'c')
elif build_machine.system() == 'darwin'
  add_project_arguments('-DOSX',      native : true, language : 'c')
elif build_machine.system() == 'linux'
  add_project_arguments('-DLINUX',    native : true, language : 'c')
else
  error('Unsupported platform')
endif

if build_machine.cpu_family() in ['aarch64', 'x86_64']
  add_project_arguments('-DOS64BIT',    native : true, language : 'c')
elif build_machine.cpu_family() in ['arm', 'x86']
  add_project_arguments('-DOS32BIT',    native : true, language : 'c')
else
  error('Unsupported platform')
endif

if get_option('RASPBERRY')
  add_project_arguments('-DRASPBERRY',  native : true, language : 'c')
endif

if get_option('DECNUMBER_FASTMUL')
  add_project_arguments('-DDECNUMBER_FASTMUL=1',  native : false, language : 'c')
endif

# DMCP arguments
add_project_arguments('-DDMCP_BUILD', native : false, language : 'c')
add_project_arguments('-DOS32BIT',    native : false, language : 'c')
if get_option('DMCPVERSION') == 'dmcp'
  if get_option('DMCP_PACKAGE') != ''
    add_project_arguments('-DDMCP_PACKAGE='+get_option('DMCP_PACKAGE'), language : 'c')
  endif
endif

git = find_program('git')
run_command(git, 'submodule', 'update', '--init', '--recursive', meson.project_source_root(), check: false)

cc = meson.get_compiler('c')
ft_dep  = dependency('freetype2', native : true)
gtk_dep = dependency('gtk+-3.0',  native : true)
gmp_dep = dependency('gmp', native : true)
m_dep   = cc.find_library('m', required : false)

if (not meson.is_cross_build()) and (host_machine.system() == 'linux' or host_machine.system() == 'darwin')
  pulse_dep = dependency('libpulse-simple', native : true, required : false)
  if pulse_dep.found()
    add_project_arguments('-DWITH_PULSEAUDIO', native : true, language : 'c')
  else
    message('libpulse-simple not found so BEEP and TONE are disabled')
  endif
else
  pulse_dep = dependency('', required : false)
endif

conf_data = configuration_data()
commit_tag = get_option('CI_COMMIT_TAG')
today = run_command('date', '+%Y-%m-%d', check: false)
conf_data.set('TODAY', today.stdout().strip())
if commit_tag != ''
  conf_data.set('CI_USE_TAG',    true)
  conf_data.set('CI_COMMIT_TAG', commit_tag)
else
  conf_data.set('CI_COMMIT_TAG', '')
endif

cvs_version_info = run_command('python3', 'tag2ver.py', check: false)
if cvs_version_info.returncode() == 0
  version_data = cvs_version_info.stdout().strip().split(';')
  conf_data.set('VERSION_WINRS',  version_data[0])
  conf_data.set('VERSION_MODDED', version_data[1])
else
  conf_data.set('VERSION_WINRS', '0,0,0,0')
  conf_data.set('VERSION_MODDED', 0)
endif

# External custom packages (optional, isolated from upstream)
# Phase 1: set the shadow include path before any subdirs.
#
# A package directory contains exactly patches/ and files/ — no
# meson.build inside a package is read or evaluated (auto-discovery,
# PROPOSED_SPEC_CHANGES.md revision 2, New Decision 3). files/*.c
# sources compile from the shadow tree like everything else, so no
# per-package -I is needed (only the shadow dir's own -I, below).
custom_pkg_list = get_option('CUSTOM_PKG')
pkg_python3 = find_program('python3', 'python')
shadow_dir = meson.project_build_root() / 'custom_pkg_shadow'

if custom_pkg_list != []
  add_project_arguments('-I' + shadow_dir, language: 'c', native: true)
  if meson.is_cross_build()
    add_project_arguments('-I' + shadow_dir, language: 'c', native: false)
  endif
endif

subdir('docs/code')
subdir('dep')
subdir('src/c47')

# Phase 2: build shadow tree and apply overrides after subdir('src/c47')
if custom_pkg_list != []
  shadow_dir = meson.project_build_root() / 'custom_pkg_shadow'
  pkg_info = run_command(pkg_python3, 'tools/resolve_c47_src.py', '--shadow', '--gen-lists',
                         meson.project_source_root() / 'src/c47/meson.build',
                         meson.project_source_root(), shadow_dir,
                         custom_pkg_list,
                         check: true)
  foreach w : pkg_info.stderr().strip().split('\n')
    if w != ''
      warning(w)
    endif
  endforeach
  lines = pkg_info.stdout().strip().split('\n')
  if lines[0].substring(0, 8) != 'INCDIRS:'
    error('Custom package resolver did not output INCDIRS line')
  endif
  inc_list = lines[0].substring(8).split(';')
  # inc_list[0] is the shadow dir (already added as -I in Phase 1);
  # rest are upstream relative dirs for c47_inc
  upstream_incs = []
  idx = 0
  foreach inc : inc_list
    if idx > 0
      upstream_incs += inc
    endif
    idx += 1
  endforeach
  set_variable('c47_inc', include_directories('custom_pkg_shadow', 'src/generateCatalogs', 'src/generateConstants', upstream_incs))
  # Source list: every entry points into the shadow dir (skip GENCAT:/GENTST:
  # generator lines and SIBSRC: sibling-root lines)
  src_lines = []
  idx = 0
  foreach line : lines
    if idx > 0 and line.substring(0, 7) != 'GENCAT:' and line.substring(0, 7) != 'GENTST:' and line.substring(0, 7) != 'SIBSRC:'
      src_lines += line
    endif
    idx += 1
  endforeach
  set_variable('c47_src', src_lines)
  # T2-A sibling roots: SIBSRC:<root>:<path> lines become
  # custom_pkg_<root>_src, consumed by src/<root>/meson.build in place
  # of its own files() list. Only testSuite is a recognized root today.
  sib_testsuite_srcs = []
  foreach line : lines
    if line.substring(0, 17) == 'SIBSRC:testSuite:'
      sib_testsuite_srcs += line.substring(17)
    endif
  endforeach
  if sib_testsuite_srcs != []
    set_variable('custom_pkg_testSuite_src', sib_testsuite_srcs)
  endif
  # F1: remap generator source lists to shadow paths
  gen_cat_srcs = []
  gen_tst_srcs = []
  foreach line : lines
    if line.substring(0, 7) == 'GENCAT:'
      gen_cat_srcs += line.substring(7)
    elif line.substring(0, 7) == 'GENTST:'
      gen_tst_srcs += line.substring(7)
    endif
  endforeach
  set_variable('generateCatalogs_src', gen_cat_srcs)
  set_variable('generateTestPgms_src', gen_tst_srcs)
endif

subdir('src/ttf2RasterFonts')
subdir('src/generateConstants')
subdir('src/generateCatalogs')
subdir('src/generateTestPgms')
subdir('src/generateTests')
subdir('src/testSuite')

# Patch-overlay tooling tests (plain-diff design, revision 2)
test('pkg_patch_common',
  find_program('python3'),
  args : ['tools/test_pkg_patch_common.py'],
  workdir : meson.project_source_root()
)

test('pkg_patch_refresh',
  find_program('python3'),
  args : ['tools/test_pkg_patch_refresh.py'],
  workdir : meson.project_source_root()
)

test('pkg_patch_resolver',
  find_program('python3'),
  args : ['tools/test_pkg_patch_resolver.py'],
  workdir : meson.project_source_root()
)

subdir('src/t47')

# Jim Tcl DSL module (only for PC_BUILD)
if not meson.is_cross_build()
  subdir('src/c47-gtk')
endif

# Only attempt to include the DMCP target if we have set-up the cross compiler
if meson.is_cross_build()
  add_pgm_chsum = files('tools/add_pgm_chsum')[0]
  modify_crc    = files('tools/modify_crc')[0]
  gen_qspi_crc  = files('tools/gen_qspi_crc')[0]
  calc_sizes    = files('tools/size.py')[0]
  py3           = find_program('python3')
  arm_objcopy   = find_program('arm-none-eabi-objcopy')
  arm_readelf   = find_program('arm-none-eabi-readelf')
  bash          = find_program('bash')
  subproject('gmp-6.2.1')
  arm_gmp_dep   = dependency('arm-gmp-6.2.1', native : false)
  if get_option('DMCPVERSION') == 'dmcp'
    subdir('src/c47-dmcp')
  endif
  if get_option('DMCPVERSION') == 'dmcp5'
    subdir('src/c47-dmcp5')
  endif
endif
