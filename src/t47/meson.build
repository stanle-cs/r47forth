# Jim Tcl DSL module
#
# Keep project-side logic minimal by delegating Jim construction to Jim's own
# build system, configured the same way we want for simulator use.

jimtcl_srcdir = meson.project_source_root() / 'dep' / 'jimtcl'
sh_prog = find_program('sh')

jim_lib = custom_target('jim-libminimal',
  input: [
    jimtcl_srcdir / 'configure',
    jimtcl_srcdir / 'Makefile.in',
    jimtcl_srcdir / 'auto.def',
    jimtcl_srcdir / 'make-bootstrap-jim',
  ],
  output: 'libjim.a',
  command: [
    sh_prog, '-c',
    'set -eu; outdir="$1"; srcdir="$2"; case "$outdir" in /*) ;; *) outdir="$PWD/$outdir" ;; esac; builddir="$outdir/jimtcl-build"; mkdir -p "$builddir"; cd "$builddir"; if ! command -v tclsh >/dev/null 2>&1; then "$srcdir/make-bootstrap-jim" > jimsh0.c && cc -o jimsh0 jimsh0.c && "$builddir/jimsh0" "$srcdir/configure" --minimal --with-ext=aio CFLAGS=-w >/dev/null; else "$srcdir/configure" --minimal --with-ext=aio CFLAGS=-w >/dev/null; fi; make libjim.a >/dev/null; cp libjim.a "$outdir/libjim.a"',
    'sh',
    '@OUTDIR@',
    jimtcl_srcdir,
  ],
  build_by_default: true,
)

# Export dependency for Jim includes and library.
# jim-config.h in this directory matches the minimal Jim configuration used for
# this project and allows Jim headers to be included by simulator sources.
t47_dep = declare_dependency(
  include_directories: [
    include_directories('.'),
    include_directories('../../dep/jimtcl'),
  ],
  sources: [jim_lib],
  link_args: [meson.current_build_dir() / 'libjim.a'],
)
