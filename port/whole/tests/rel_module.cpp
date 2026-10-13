// Test REL module "re4test_rel" for re4_host --runtime-test: SN-style entry
// points, initialised and zeroed statics to observe whether OSLink kept or
// reset the module, and a static object whose destructor OSUnlink must run.
static int gPrologCalls;  // .bss
static int gData = 7;     // .data
static int* gDtorCounter;

namespace {
struct Witness {
    ~Witness()
    {
        if (gDtorCounter) {
            ++*gDtorCounter;
        }
    }
};
Witness gWitness;
}  // namespace

extern "C" void _prolog()
{
    gPrologCalls++;
}

extern "C" void _epilog() {}

extern "C" void _unresolved() {}

extern "C" int re4test_rel_state()
{
    return gPrologCalls * 100 + gData;
}

extern "C" void re4test_rel_bump()
{
    gData++;
}

extern "C" void re4test_rel_set_dtor_counter(int* counter)
{
    gDtorCounter = counter;
}
