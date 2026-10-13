// Trampoline self-test, provider side (module "re4test_target"): a function
// with more arguments than fit in registers, mixed integer and floating point,
// so a trampoline that disturbs registers or the stack changes the result.
extern "C" double re4test_mix(int a, double b, int c, float d, int e, int f, double g, int h,
                              float i, int j)
{
    return a * 1.0 + b * 10.0 + c * 100.0 + d * 1000.0 + e * 1e4 + f * 1e5 + g * 1e6 + h * 1e7 +
           i * 1e8 + j * 1e9;
}

extern "C" long long re4test_wide(long long x, int y)
{
    return x * 3 + y;
}
