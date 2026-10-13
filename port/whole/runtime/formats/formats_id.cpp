// Formatumwandler: screen layouts of the id system (UWF, src/game/id_sys.cpp
// IDSystem::set): ID_FILE_HEADER, UnitNum entries (ID_DATA_V1 for version
// "1.00", ID_DATA_V2 otherwise) and the data their offsets point at: a
// B-spline (path0, FUNC_PATH_PTR), its work area (path1, written by
// FuncPathParametrize) and four Hermite curves (HERMITE_1_PTR).
#include "formats.h"

#include "hermite.h"
#include "id_sys.h"
#include "path.h"
#include "re4_runtime.h"

#include <stdlib.h>

using namespace re4fmt;

static_assert(sizeof(ID_FILE_HEADER) == 8, "ID_FILE_HEADER layout");
static_assert(sizeof(ID_DATA_V1) == 0x88, "ID_DATA_V1 layout");
static_assert(sizeof(ID_DATA_V2) == 0x8C, "ID_DATA_V2 layout");
static_assert(offsetof(FUNC_PATH_PTR, Point) == 8, "FUNC_PATH_PTR layout");
static_assert(sizeof(HERMITE_1_POINT) == 0x10, "HERMITE_1_POINT layout");

namespace re4fmt {

namespace {

void convertFuncPath(Blob& b, u32 o)
{
    if (!fits(b, o, offsetof(FUNC_PATH_PTR, Point), "FUNC_PATH_PTR")) {
        return;
    }
    s8 n = (s8) b.data[o + offsetof(FUNC_PATH_PTR, nPoint)];
    if (n > 0) {
        swap32(b, o + offsetof(FUNC_PATH_PTR, Point), (u32) n * 3);
    }
}

void convertHermite(Blob& b, u32 o)
{
    s32 n = (s32) be32(b, o);
    swap32(b, o);
    if (n > 0 && b.ok) {
        swap32(b, o + offsetof(HERMITE_1_PTR, Point), (u32) n * (sizeof(HERMITE_1_POINT) / 4));
    }
}

template <class T>
void convertEntry(Blob& b, u32 o, Seen& seen)
{
    if (!fits(b, o, sizeof(T), "id entry")) {
        return;
    }
    swap32(b, o + offsetof(T, pos), 3);  // Vec: three f32
    swap32(b, o + offsetof(T, vtx), 12);
    RE4_SWAP(b, o, T, sizeX);
    RE4_SWAP(b, o, T, sizeY);
    swap32(b, o + offsetof(T, rot), 3);
    u32 ofs[6];
    for (int i = 0; i < 6; i++) {
        ofs[i] = be32(b, o + offsetof(T, ofs) + i * 4);
    }
    RE4_SWAP_ARRAY(b, o, T, ofs);
    // path1 is FuncPathParametrize's output: nothing to convert.
    if (ofs[0] && seen.first(ofs[0])) {
        convertFuncPath(b, ofs[0]);
    }
    for (int i = 2; i < 6; i++) {
        if (ofs[i] && seen.first(ofs[i])) {
            convertHermite(b, ofs[i]);
        }
    }
}

}  // namespace

void convertIdLayout(Blob& b, const Part&)
{
    if (!fits(b, 0, sizeof(ID_FILE_HEADER), "ID_FILE_HEADER")) {
        return;
    }
    char ver[5] = { (char) b.data[0], (char) b.data[1], (char) b.data[2], (char) b.data[3], 0 };
    int v = (int) (float) strtod(ver, 0);
    u32 n = b.data[offsetof(ID_FILE_HEADER, UnitNum)];
    Seen seen;
    for (u32 i = 0; i < n && b.ok; i++) {
        if (v == 1) {
            convertEntry<ID_DATA_V1>(b, sizeof(ID_FILE_HEADER) + i * sizeof(ID_DATA_V1), seen);
        } else {
            convertEntry<ID_DATA_V2>(b, sizeof(ID_FILE_HEADER) + i * sizeof(ID_DATA_V2), seen);
        }
    }
}

}  // namespace re4fmt
