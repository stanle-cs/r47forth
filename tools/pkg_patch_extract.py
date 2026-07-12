"""Function-boundary extractor using libclang (clang.cindex).

Authoring-time tool only — must NEVER be imported by build-time tooling
(resolve_c47_src.py or anything invoked by meson/ninja).  See §4, ratified.

Requires a vanilla build's compile_commands.json (CUSTOM_PKG unset) so that
file paths resolve to src/c47/<rel>, not to custom_pkg_shadow/ paths.
"""
import json
import os

import clang.cindex

# Ensure libclang shared library is locatable.  The pip 'libclang' package
# ships its own copy, but on systems with a system libclang (e.g. Ubuntu
# libclang-18-dev), pointing to the system library is more reliable.
if clang.cindex.Config.library_file is None:
    _SYSTEM_LIBCLANG = '/usr/lib/x86_64-linux-gnu/libclang-18.so'
    if os.path.isfile(_SYSTEM_LIBCLANG):
        clang.cindex.Config.library_file = _SYSTEM_LIBCLANG


def _filter_build_flags(args, entry_dir):
    """Filter build-system flags from compile args and resolve relative paths.

    compile_commands.json entries contain the full compiler invocation,
    including flags like -MD, -MQ, -MF, -o, -c and the source file itself.
    libclang only needs preprocessor/compiler flags (-I, -D, -W, etc.).

    Also resolves relative -I paths (relative to entry_dir) to absolute.
    """
    # Flags that take a following argument — skip both the flag and its value
    _SKIP_WITH_ARG = {'-MF', '-MQ', '-o'}
    # Flags that stand alone — skip just the flag
    _SKIP_STANDALONE = {'-MD', '-MMD', '-MP', '-c'}
    # File extensions that indicate a source file argument
    _SRC_EXTS = ('.c', '.cpp', '.cc', '.cxx', '.m', '.mm')

    resolved = []
    skip_next = False
    for arg in args:
        if skip_next:
            skip_next = False
            continue
        if arg in _SKIP_WITH_ARG:
            skip_next = True
            continue
        if arg in _SKIP_STANDALONE:
            continue
        # Skip the source file argument (last positional arg)
        if any(arg.endswith(ext) for ext in _SRC_EXTS):
            continue
        # Resolve relative -I paths
        if arg.startswith('-I') and len(arg) > 2 and not os.path.isabs(arg[2:]):
            resolved.append('-I' + os.path.realpath(os.path.join(entry_dir, arg[2:])))
        elif arg.startswith('-I=') and len(arg) > 3 and not os.path.isabs(arg[3:]):
            resolved.append('-I=' + os.path.realpath(os.path.join(entry_dir, arg[3:])))
        else:
            resolved.append(arg)
    return resolved


def _find_compile_args(file_path, compile_commands_json_path):
    """Locate the compile args list for *file_path* in *compile_commands_json_path*.

    Returns the list of compiler arguments suitable for libclang parsing
    (build-system flags removed, relative paths resolved), or raises
    FileNotFoundError if no matching entry exists.
    """
    file_real = os.path.realpath(file_path)

    with open(compile_commands_json_path, 'r') as f:
        entries = json.load(f)

    for entry in entries:
        entry_dir = entry.get('directory', '.')
        entry_file = os.path.join(entry_dir, entry['file'])
        if os.path.realpath(entry_file) == file_real:
            cmd = entry['command']
            # Parse the command: split on whitespace, skip the compiler binary
            parts = cmd.split()
            # The first token is the compiler (cc, gcc, clang, etc.)
            raw_args = parts[1:]
            return _filter_build_flags(raw_args, entry_dir)

    raise FileNotFoundError(
        f'no compile_commands.json entry for {file_path!r} '
        f'(resolved: {file_real!r}) in {compile_commands_json_path!r}'
    )


def list_function_ranges(file_path, compile_commands_json_path):
    """Return a list of (name: str, start_line: int, end_line: int),
    1-indexed inclusive, for every function *definition* (not
    declaration/prototype) in file_path, using clang.cindex against the
    real compile args for file_path found in compile_commands_json_path.

    - Uses clang.cindex.Index.create() and Index.parse() with the args
      list resolved from the compile_commands.json entry whose 'file'
      field matches os.path.realpath(file_path). Fatal error (raise, do
      not fall back to guessing flags) if no matching entry exists.
    - Walks translation_unit.cursor.walk_preorder(), filtering
      cursor.kind == clang.cindex.CursorKind.FUNCTION_DECL and
      cursor.is_definition() and cursor.location.file is not None and
      os.path.realpath(cursor.location.file.name) == os.path.realpath(file_path)
      (the file-identity check matters: libclang will walk into included
      headers otherwise, e.g. defines.h, and you do not want their
      functions in this file's range list).
    - start_line/end_line come from cursor.extent.start.line /
      cursor.extent.end.line — this is libclang's actual parsed AST
      extent, which is correct for braces inside string/char literals and
      for macro-expanded bodies (the extent reflects the expansion, not
      raw token brace-counting), and for #ifdef-guarded bodies clang
      parses only the active branch as determined by the same
      preprocessor defines used in the real build (from
      compile_commands.json), so an inactive #ifdef branch's braces never
      appear in the AST at all — do not add separate #ifdef handling on
      top of this; the compile args already resolve it correctly.
    """
    file_path = str(file_path)
    compile_commands_json_path = str(compile_commands_json_path)

    if not os.path.isfile(compile_commands_json_path):
        raise FileNotFoundError(
            f'compile_commands.json not found at {compile_commands_json_path!r}. '
            f'Configure a vanilla build first (no CUSTOM_PKG) per BUILD.md.'
        )

    args = _find_compile_args(file_path, compile_commands_json_path)

    index = clang.cindex.Index.create()
    tu = index.parse(file_path, args,
                     options=clang.cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)

    file_real = os.path.realpath(file_path)
    ranges = []

    for cursor in tu.cursor.walk_preorder():
        if (cursor.kind == clang.cindex.CursorKind.FUNCTION_DECL
                and cursor.is_definition()
                and cursor.location.file is not None
                and os.path.realpath(cursor.location.file.name) == file_real):
            name = cursor.spelling
            if not name:
                continue
            start_line = cursor.extent.start.line
            end_line = cursor.extent.end.line
            ranges.append((name, start_line, end_line))

    ranges.sort(key=lambda r: r[1])
    return ranges


def function_at_line(ranges, line):
    """Given the list from list_function_ranges and a 1-indexed line
    number, return the (name, start, end) tuple whose range contains
    line, or None if no function's range contains it (global/macro/struct/
    file-scope code)."""
    for name, start, end in ranges:
        if start <= line <= end:
            return (name, start, end)
    return None
