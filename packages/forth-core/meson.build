# forth-core package manifest (rebuild — grows as sources are regenerated)
pkg_override_sources = ['config.c', 'items.c', 'programming/lblGtoXeq.c']
pkg_override_headers = ['items.h']
pkg_custom_sources   = files('forth_dict.c', 'forth_prims.c', 'forth_inner.c', 'forth_compile.c', 'forth_bridge.c', 'test_dict_reloc.c')
add_project_arguments('-DFORTH_DEBUG_SELFTEST', language: 'c', native: true)
