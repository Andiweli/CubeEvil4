// Self-test of the milestone 12 checks (port/tools/m12/selftest/run_selftest.py): one
// example of every error form the checks look for, each marked `// expect: CHECK`, and
// the reviewed forms of the port (RE4_OFS, RE4_OWN, RE4_LP64_SIZE, sizeof), which no
// check may report. Compiled for armhf (ILP32, the reference) and x86_64 (LP64); never
// part of the game.
#include <string.h>

typedef unsigned char u8;
typedef unsigned int u32;
typedef int s32;

void* mem_alloc(u32 size, const char* file, int line, int flag, int heap);
#define MEM_ALLOC(size, a, b) mem_alloc(size, __FILE__, __LINE__, a, b)
extern "C" void memclr_asm(void* p, u32 n);
void getPtr(void** out);

struct Wide {     // 0x0C bytes on ILP32, 0x18 on LP64
    s32 a;        // 0x00
    void* p;      // 0x04 (LP64 0x08)
    s32 b;        // 0x08 (LP64 0x10)
};
struct Narrow {   // a view of Wide with GameCube offsets
    s32 a;
    s32 pad;
    s32 b;
};
struct Holder {
    s32 flags;
    Wide w;
    u8 free[0x10];
};
struct Base {
    virtual ~Base();
    s32 x;
};
struct Derived : Base {
    s32 own;
};
struct DiscHead {   // laid over disc data: an offset that becomes a pointer in place
    s32 count;
    Wide* table;
};
struct Owner {
    void* pWork;   // a Wide in one place, read at the GameCube offset of Wide::b in another
};
struct EspBase {  // 0x10 bytes on ILP32; on LP64 its data ends at 0x1C, sizeof 0x20 (tail padding)
    virtual ~EspBase();
    s32 x;
    void* p;
    s32 y;
};
struct EspNear : EspBase {  // own work at 0x1C on LP64 (in EspBase's tail padding)
    s32 flags;
};
struct EspFar : EspBase {   // own work at 0x20 on LP64 (it holds a pointer: 8-aligned)
    void* path;
    s32 flags;
};

#define VIEW(h) ((Narrow*) &(h)->w)          // a view over a member, in a macro (MODEL_TEX)
#define FIELD_B(p) (*(s32*) ((u8*) (p) + 8))  // a GameCube byte offset in a macro
#define CLEAR_WIDE(p) memclr_asm((p), 0xC)    // a GameCube length in a macro
#define WIDE_SIZE RE4_LP64_SIZE(0xC, sizeof(Wide))  // reviewed sizes and offsets inside other macros
#define WIDE_B RE4_OFS(Wide, b, 8)
#define ADDR32(x) RE4_ADDR32(x)

s32 g_value;
void* myAlloc(u32 n) { return MEM_ALLOC(n, 1, 13); }   // an allocator of the program (a wrapper)
void* myAlloc2(u32 n) { return myAlloc(n + 0x20); }    // and a wrapper of it
u8 g_table[0xC];                  // decl_size_check: forms_b.cpp declares Wide2 g_table[1]
extern "C" void takes(Wide* w);   // proto_check: forms_b.cpp has takes(Narrow2*)
struct Shared {                   // view_check: forms_b.cpp has a view with a GameCube pad
    s32 a;
    void* p;
    s32 b;
};
s32 useShared(Shared* s) { takes(0); return s->b; }

void forms2(EspBase* e, u8* buf, Owner* o)
{
    Wide* ow = (Wide*) o->pWork;
    g_value = *(s32*) ((u8*) o->pWork + 8);          // expect: work_view_check
    g_value = ((s32*) o->pWork)[2];                  // expect: work_view_check
    (void) ow;
    Narrow nw;
    memcpy(&nw, ((Holder*) buf)->w.p ? &((Holder*) buf)->w : 0, sizeof(Narrow));  // expect: copy_view_check
    memcpy(buf, e, 0x10);                            // expect: copy_view_check
    g_value = nw.b;
    DiscHead* dh = (DiscHead*) buf;
    dh->table = (Wide*) ((u8*) dh->table + (u32) (unsigned long) dh);  // expect: reloc_check
    g_value = *(s32*) ((u8*) e + sizeof(EspBase));   // expect: sizeof_ptr_check
    g_value = *(s32*) (buf + sizeof(Wide));          // expect: sizeof_ptr_check
    g_value = *(s32*) ((u8*) e + RE4_OWN(EspBase, s32, 0x10));  // expect: own_start_check
}

void forms(Holder* h, Wide* w, s32 n, u8* pool, int i)
{
    char* c1 = (char*) n;                            // expect: int_cast_check
    s32 slot;
    getPtr((void**) &slot);                          // expect: ptr_slot_check
    *(u32*) &w->p = 0;                               // expect: ptr_word_check
    {
        u32* link = (u32*) &w->p;                    // the word view in a variable (EfmSetObj04)
        link[0] = 0;                                 // expect: ptr_word_check
        link[1] = 7;                                 // expect: ptr_word_check
        *((u32*) &w->p + 1) = 0;                     // expect: ptr_word_check
    }
    Narrow* v1 = (Narrow*) w;                        // expect: cast_check
    Narrow* v2 = VIEW(h);                            // expect: cast_check
    g_value = FIELD_B(w);                            // expect: raw_offset_check raw_offset_words
    g_value = *(s32*) ((u8*) w + (n * 12 + 8));      // expect: raw_offset_check raw_offset_words
    g_value = ((s32*) w)[2];                         // expect: raw_offset_words
    g_value = *(s32*) (n * sizeof(Wide) + (u32) (unsigned long) h + 0x14);  // expect: raw_offset_check raw_offset_words sizeof_ptr_check
    {
        u32 base = n * 4 + (u32) (unsigned long) h;
        base += 0x14;                                // expect: raw_offset_check raw_offset_words
        g_value = *(s32*) (unsigned long) base;
    }
    Wide* a1 = (Wide*) MEM_ALLOC(0xC, 1, 13);        // expect: alloc_size_check
    void** a2 = (void**) MEM_ALLOC(n * 4, 1, 13);    // expect: alloc_size_check
    memclr_asm(w, 0xC);                              // expect: alloc_size_check
    Wide* a3 = (Wide*) (pool + i * 0xC);             // expect: alloc_size_check
    memset(&w->a, 0, 8);                             // expect: span_check
    g_value = h->free[0x14];                         // expect: array_view_check
    Wide* o1 = (Wide*) h->free;                      // expect: overlay_check overlay_areas
    CLEAR_WIDE(w);                                   // expect: alloc_size_check
    Wide* a7 = (Wide*) myAlloc2(0xC);                // expect: alloc_size_check
    Wide* a8 = (Wide*) new u8[0xC];                  // expect: alloc_size_check
    (void) a7; (void) a8;
    (void) c1; (void) v1; (void) v2; (void) a1; (void) a2; (void) a3; (void) o1;
}

// The reviewed forms: no check may report these.
void reviewed(Holder* h, Wide* w, s32 n, u8* pool, int i, Derived* d)
{
    char* c1 = (char*) RE4_ADDR32(n);
    g_value = *(s32*) ((u8*) w + RE4_OFS(Wide, b, 8));
    g_value = *(s32*) ((u8*) w + (n * 12 + RE4_OFS(Wide, b, 8)));
    g_value = *(s32*) ((u8*) d + RE4_OWN(Base, s32, 8));
    Wide* a1 = (Wide*) MEM_ALLOC(RE4_LP64_SIZE(0xC, sizeof(Wide)), 1, 13);
    Wide* a2 = (Wide*) MEM_ALLOC(sizeof(Wide), 1, 13);
    void** a3 = (void**) MEM_ALLOC(n * sizeof(void*), 1, 13);
    memclr_asm(w, sizeof(Wide));
    memclr_asm(w, RE4_LP64_SIZE(0xC, 0x18));
    Wide* a4 = (Wide*) (pool + i * sizeof(Wide));
    Wide* a5 = (Wide*) (pool + i * RE4_LP64_SIZE(0xC, 0x18));
    memset(&w->a, 0, sizeof(w->a));
    Wide* a6 = (Wide*) MEM_ALLOC(WIDE_SIZE, 1, 13);
    g_value = *(s32*) ((u8*) w + WIDE_B);
    g_value = *(s32*) ((u8*) w + (n * 12 + WIDE_B));
    char* c2 = (char*) ADDR32(n);
    u32* own = (u32*) &w->a;                         // a word view of a word: no pointer behind it
    own[0] = 1;
    (void) a6; (void) c2;
    (void) c1; (void) a1; (void) a2; (void) a3; (void) a4; (void) a5; (void) h;
}
