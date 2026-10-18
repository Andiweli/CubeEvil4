// Second unit of the self-test (forms.cpp): forms that need two units.
typedef unsigned char u8;
typedef int s32;

struct Shared {      // expect: view_check=Shared (forms.cpp has the full view)
    u8 pad[8];
    s32 b;
};
struct Narrow2 {
    s32 a;
    s32 pad;
    s32 b;
};
struct Wide2 {
    s32 a;
    void* p;
    s32 b;
};
extern Wide2 g_table[1];               // expect: decl_size_check=g_table (forms.cpp: u8 g_table[0xC])
extern "C" void takes(Narrow2* w);     // expect: proto_check=takes (forms.cpp: takes(Wide*))

s32 useB(Shared* s)
{
    takes(0);
    return s->b + g_table[0].b;
}
