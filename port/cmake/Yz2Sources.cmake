# Only mechanical ABI/state/bounds/checkpoint changes to the reviewed C setup.
# The PPC kernel is implemented separately from yz2asm.cpp's instructions.
function(re4_prepare_yz2 root destination)
    file(READ "${root}/src/game/yz2code.cpp" source)
    string(REPLACE "\r\n" "\n" source "${source}")
    string(FIND "${source}" "struct Yz2InEv {" start)
    string(FIND "${source}" "// Parses the yz2 header" end)
    math(EXPR count "${end} - ${start}")
    string(SUBSTRING "${source}" ${start} ${count} structs)
    string(REPLACE "static Yz2InEv in_ev;" "" structs "${structs}")
    string(REPLACE "u32 size1;    // 0x10  second hex field of the header string"
        "u32 size1;    // Original scalar; following members are native adapter state.\n    const u8* srcEnd;\n    u8* workEnd;\n    Yz2Checkpoint checkpoint;\n    Yz2Statistics* statistics;" structs "${structs}")
    file(CONFIGURE OUTPUT "${destination}/include/yz2_structs.inc" CONTENT
        "// Original field definitions; offsets in comments describe PPC only.\n${structs}" @ONLY)
    string(SUBSTRING "${source}" ${end} -1 bodies)
    string(REPLACE "u32 Yz2DecodeSet(char* str, void* buf)"
        "u32 Yz2DecodeSet(char* str, void* buf, Yz2InEv& in_ev)" bodies "${bodies}")
    string(REPLACE "void Yz2DecodeExec(void* dst)"
        "void Yz2DecodeExec(void* dst, Yz2InEv& in_ev)" bodies "${bodies}")
    string(REPLACE "static inline void* yz2Alloc(u32 size)"
        "static inline void* yz2Alloc(Yz2InEv& in_ev, u32 size)" bodies "${bodies}")
    string(REPLACE "void* p = in_ev.free;\n    in_ev.free += size;\n    return p;"
        "return yz2Bump(in_ev, size);" bodies "${bodies}")
    string(REPLACE "yz2Alloc((cnt)" "yz2Alloc(in_ev, (cnt)" bodies "${bodies}")
    string(REPLACE "yz2Alloc(0x" "yz2Alloc(in_ev, 0x" bodies "${bodies}")
    string(REPLACE "(((u32) p + 0x20) & ~0x1F)"
        "((reinterpret_cast<std::uintptr_t>(p) + 0x20u) & ~std::uintptr_t{0x1F})" bodies "${bodies}")
    string(REPLACE "(Yz2DicEnt*) (i * sizeof(Yz2DicEnt) + (u32) c->dic)"
        "c->dic + i" bodies "${bodies}")
    string(REPLACE "*in_ev.src++" "yz2ReadByte(in_ev)" bodies "${bodies}")
    # Preserve model math and work layout. Bounded loops may pause, retaining
    # their original locals. No allocation/lifetime is hidden in these hooks.
    string(REPLACE "f->freq[i] = 1;" "yz2Pulse(in_ev, 1); f->freq[i] = 1;" bodies "${bodies}")
    string(REPLACE "fd->freq[j]++;" "yz2Pulse(in_ev, 1); fd->freq[j]++;" bodies "${bodies}")
    string(REPLACE "fd->cum[i * 2] =" "yz2Pulse(in_ev, 1); fd->cum[i * 2] =" bodies "${bodies}")
    string(REPLACE "p->cnt = 0;" "yz2Pulse(in_ev, sizeof(Yz2DicEnt)); p->cnt = 0;" bodies "${bodies}")
    file(CONFIGURE OUTPUT "${destination}/yz2_setup.cpp" CONTENT
        "// Generated from src/game/yz2code.cpp; adaptations: Yz2Sources.cmake.\n#include \"yz2_internal.h\"\n#include <cstring>\n#include <cstdlib>\nnamespace re4::port::detail {\n${bodies}\n}\n" @ONLY)
    re4_extract_function("${root}/src/game/dvd.cpp" "void DvdReadProc()" dvd_definition)
    file(CONFIGURE OUTPUT "${destination}/dvd_adapter_probe.cpp" CONTENT
        "// VERIFICATION ONLY: actual DvdReadProc wrapper, isolated native delegate.\n#include \"dvd_adapter_probe.h\"\nnamespace re4::port::verification {\n${dvd_definition}\n}\n" @ONLY)
endfunction()
