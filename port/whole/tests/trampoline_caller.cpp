// Trampoline self-test, caller side (module "re4test_caller"): calls the
// target module only through the generated trampolines (imports/re4test_caller.S).
extern "C" double re4test_mix(int a, double b, int c, float d, int e, int f, double g, int h,
                              float i, int j);
extern "C" long long re4test_wide(long long x, int y);

extern "C" int re4test_run()
{
    double r = re4test_mix(1, 2.0, 3, 4.0f, 5, 6, 7.0, 8, 9.0f, 1);
    double expect = 1 + 20 + 300 + 4000 + 5e4 + 6e5 + 7e6 + 8e7 + 9e8 + 1e9;
    long long w = re4test_wide(0x100000001LL, -7);
    return (r == expect ? 0 : 1) | (w == 0x300000003LL - 7 ? 0 : 2);
}
