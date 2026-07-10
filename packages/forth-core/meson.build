# forth-core package manifest (rebuild — grows as sources are regenerated)
pkg_override_sources = ['config.c', 'error.c', 'items.c', 'screen.c', 'programming/lblGtoXeq.c', 'ui/tam.c', 'keyboard.c']
pkg_override_headers = ['items.h']
pkg_custom_sources   = files('forth_dict.c', 'forth_prims.c', 'forth_inner.c', 'forth_compile.c', 'forth_bridge.c', 'test_dict_reloc.c')

# Fix #15: gate FORTH_DEBUG_SELFTEST behind a meson OPTION (not always-on).
# The option is declared in the top-level meson_options.txt; default is true.
if get_option('FORTH_DEBUG_SELFTEST')
  add_project_arguments('-DFORTH_DEBUG_SELFTEST', language: 'c', native: true)
endif
