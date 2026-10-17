// Mixed benchmark in the style of the game's frame work: C++ objects with
// virtual move() in linked lists (cManager), float matrix math as in the
// SDK's C_MTX functions (skinning, world matrices), a byte decoder loop
// (archive decompression) and memory copies. Pure computation, no library:
// the same source runs natively and inside the wasm32 sandbox (wasm2c).
typedef unsigned int u32;
typedef unsigned char u8;
typedef float f32;
extern "C" void* memcpy(void* d, const void* s, unsigned long n);
extern "C" void* memset(void* d, int c, unsigned long n);
#ifdef __wasm__
extern "C" void* memcpy(void* d, const void* s, unsigned long n) { return __builtin_memcpy(d, s, n); }
extern "C" void* memset(void* d, int c, unsigned long n) { return __builtin_memset(d, c, n); }
#endif

typedef f32 Mtx[3][4];

static void mtxConcat(const Mtx a, const Mtx b, Mtx ab)
{
    Mtx t;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            t[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] + (j == 3 ? a[i][3] : 0.0f);
        }
    }
    memcpy(ab, t, sizeof(Mtx));
}

static void mtxRotY(Mtx m, f32 s, f32 c)
{
    m[0][0] = c; m[0][1] = 0; m[0][2] = s; m[0][3] = 0;
    m[1][0] = 0; m[1][1] = 1; m[1][2] = 0; m[1][3] = 0;
    m[2][0] = -s; m[2][1] = 0; m[2][2] = c; m[2][3] = 0;
}

struct Vec { f32 x, y, z; };

struct cUnit {
    cUnit* next;
    u32 flag;
    virtual ~cUnit() {}
    virtual void move() = 0;
};

struct cParts {
    Mtx mat;
    Mtx l_mat;
    cParts* parent;
};

struct cModel : cUnit {
    Vec pos, spd;
    f32 ang;
    cParts parts[24];
    int nParts;
    u32 timer;
    void move()
    {
        pos.x += spd.x; pos.y += spd.y; pos.z += spd.z;
        if (pos.y < 0) { pos.y = 0; spd.y = -spd.y * 0.5f; }
        spd.y -= 0.98f;
        ang += 0.01f;
        f32 s = ang - ang * ang * ang / 6.0f, c = 1.0f - ang * ang / 2.0f;
        Mtx root;
        mtxRotY(root, s, c);
        root[0][3] = pos.x; root[1][3] = pos.y; root[2][3] = pos.z;
        for (int i = 0; i < nParts; i++) {
            const f32(*pm)[4] = parts[i].parent ? parts[i].parent->mat : root;
            mtxConcat(pm, parts[i].l_mat, parts[i].mat);
        }
        if (ang > 1.0f) ang -= 2.0f;
        timer++;
    }
};

struct cEnemy : cModel {
    int hp;
    void move()
    {
        cModel::move();
        if ((timer & 63) == 0) hp--;
        if (hp < 0) hp = 100;
    }
};

struct cObject : cModel {
    void move()
    {
        if (timer & 1) cModel::move(); else timer++;
    }
};

static u8 arena[(sizeof(cEnemy) + 16) * 160];
static u8 src[1 << 16], dst[1 << 18];

// placement new without the library
inline void* operator new(unsigned long, void* p) { return p; }
void operator delete(void*) noexcept {}

static u32 decode(const u8* in, u32 n, u8* out)
{
    // LZ-like byte decoder: literal runs and back references
    u32 o = 0, i = 0;
    while (i + 3 < n && o + 300 < sizeof(dst)) {
        u8 c = in[i++];
        if (c < 0x80) {
            u32 len = (c & 0x3F) + 1;
            for (u32 k = 0; k < len && i < n; k++) out[o++] = in[i++];
        } else {
            u32 len = (c & 0x1F) + 3;
            u32 dist = ((u32) in[i] << 8 | in[i + 1]) % (o + 1) + 1;
            i += 2;
            for (u32 k = 0; k < len; k++, o++) out[o] = out[o - (dist <= o ? dist : o)];
        }
    }
    return o;
}

extern "C" u32 bench_run(u32 frames)
{
    cUnit* list = 0;
    u8* p = arena;
    for (int i = 0; i < 160; i++) {
        cModel* m;
        if (i % 3 == 0) {
            cObject* o = new (p) cObject();
            m = o;
        } else {
            cEnemy* e = new (p) cEnemy();
            e->hp = 100;
            m = e;
        }
        p += sizeof(cEnemy) + 16;
        m->pos.x = (f32) i; m->pos.y = 10.0f; m->pos.z = 0;
        m->spd.x = 0.1f; m->spd.y = 0; m->spd.z = 0.05f;
        m->ang = 0;
        m->timer = 0;
        m->nParts = 24;
        for (int k = 0; k < 24; k++) {
            mtxRotY(m->parts[k].l_mat, 0.1f, 0.995f);
            m->parts[k].l_mat[1][3] = 10.0f;
            m->parts[k].parent = k ? &m->parts[(k - 1) / 2] : 0;
        }
        m->next = list;
        list = m;
    }
    u32 seed = 1;
    for (u32 i = 0; i < sizeof(src); i++) {
        seed = seed * 1103515245u + 12345u;
        src[i] = (u8) (seed >> 16);
    }
    u32 sum = 0;
    for (u32 f = 0; f < frames; f++) {
        for (cUnit* u = list; u; u = u->next) {
            u->move();
        }
        sum += decode(src, sizeof(src) / 8, dst);
        memcpy(dst + 1024, dst, 32768);
        memset(dst, (int) f, 4096);
        sum += dst[f & 4095] + dst[1024 + (f & 1023)];
    }
    cModel* m = (cModel*) list;
    f32 chk = m->parts[23].mat[0][3] + m->parts[23].mat[1][3];
    return sum + (u32) (chk * 100.0f);
}
