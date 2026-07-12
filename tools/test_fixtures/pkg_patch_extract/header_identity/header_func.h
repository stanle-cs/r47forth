#ifndef HEADER_FUNC_H
#define HEADER_FUNC_H

// This function is defined in the header — it should NOT appear
// when parsing main.c, only when parsing this header directly.
int header_function(void) {
    return 42;
}

#endif
