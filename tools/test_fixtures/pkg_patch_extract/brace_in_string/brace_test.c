// Test fixture: function with braces inside string literals.
// A naive brace counter would stop at the '}' inside the string,
// but libclang's AST extent correctly identifies the real closing brace.

void func_with_braces_in_string(void) {
    const char *s1 = "hello { world }";
    const char *s2 = "nested { { braces } } here";
    char c = '}';
    // Real closing brace is below, not the ones in the strings above
}

int simple_func(void) {
    return 42;
}
