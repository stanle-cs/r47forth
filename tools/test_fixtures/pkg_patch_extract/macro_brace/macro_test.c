// Test fixture: macro-expanded braces in and near a target function.
// BEGIN_BODY / END_BODY expand to '{' / '}' tokens; a raw-token brace
// scanner sees no literal brace on the definition line at all, but
// libclang's AST extent reflects the parsed expansion.

#define BEGIN_BODY {
#define END_BODY }
#define EMPTY_STMT_BLOCK { }

int macro_brace_func(int x)
BEGIN_BODY
    if (x > 0) EMPTY_STMT_BLOCK
    return x * 2;
END_BODY

int following_func(void) {
    return 7;
}
