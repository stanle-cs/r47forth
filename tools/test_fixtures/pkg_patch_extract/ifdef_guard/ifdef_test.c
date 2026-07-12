// Test fixture: function definitions guarded by #ifdef.
// The active branch is determined by the compile flags in
// compile_commands.json (-DFEATURE_ON), exactly as in a real build.

#ifdef FEATURE_ON
int guarded_function(int x) {
    return x + 1;
}
#else
int guarded_function(int x) {
    return x - 1;
}
#endif

int after_func(void) {
#ifdef FEATURE_ON
    return 1;
#else
    return 0;
#endif
}
