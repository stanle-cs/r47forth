#include "header_func.h"

// This function is defined in main.c — it SHOULD appear
// when parsing main.c.
int main_function(void) {
    return header_function() + 1;
}
