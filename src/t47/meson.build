# Jim Tcl DSL module - builds Jim as static library, dsl.c added to c47-gtk sources

jimtcl_srcdir = meson.project_source_root() / 'dep' / 'jimtcl'

# Generate C files from Tcl extensions using Jim's make-c-ext.tcl
generated_sources = []
tcl_extensions = ['glob', 'nshelper', 'oo', 'stdlib', 'tclcompat', 'tree']
foreach ext : tcl_extensions
  generated = custom_target('jim-' + ext + '.c',
    input: jimtcl_srcdir / (ext + '.tcl'),
    output: ext + '.c',
    command: ['/usr/local/bin/tclsh', '@INPUT@'],
    capture: true,
    output_dir: meson.current_build_dir(),
  )
  generated_sources += generated
endforeach

# Generate _load-static-exts.c using make-load-static-exts.tcl
load_static_exts = custom_target('_load-static-exts.c',
  input: jimtcl_srcdir / 'make-load-static-exts.tcl',
  output: '_load-static-exts.c',
  command: ['/usr/local/bin/tclsh', '@INPUT@',
    'aio', 'array', 'clock', 'eventloop', 'exec', 'file', 'glob', 'history', 'interp', 'load',
    'namespace', 'nshelper', 'oo', 'pack', 'package', 'posix', 'readdir', 'regexp', 'signal', 'stdlib',
    'syslog', 'tclcompat', 'tree'],
  capture: true,
  output_dir: meson.current_build_dir(),
)
generated_sources += load_static_exts

# Jim Tcl core and extension sources
jim_sources = [
  # Core Jim files
  '../../dep/jimtcl/jim.c',
  '../../dep/jimtcl/jimiocompat.c',
  '../../dep/jimtcl/jim-subcmd.c',
  '../../dep/jimtcl/utf8.c',
  '../../dep/jimtcl/jim-format.c',
  # Generated sources
] + generated_sources

# Additional C extension files that exist in dep/jimtcl
jim_c_extensions = [
  'jim-tty.c', 'openpty.c',
  'jim-aio.c', 'jim-array.c', 'jim-clock.c', 'jim-eventloop.c',
  'jim-exec.c', 'jim-file.c', 'jim-history.c', 'jim-interp.c',
  'jim-load.c', 'jim-namespace.c', 'jim-pack.c', 'jim-package.c',
  'jim-posix.c', 'jim-readdir.c', 'jim-regexp.c', 'jim-signal.c',
  'jim-syslog.c',
]
foreach ext : jim_c_extensions
  jim_sources += join_paths('..', '..', 'dep', 'jimtcl', ext)
endforeach

# Build Jim as a static library
jim_lib = static_library('jim',
  sources: jim_sources,
  include_directories: [include_directories('.')],
)

# Export dependency for Jim includes and library
t47_dep = declare_dependency(
  link_with: jim_lib,
)
