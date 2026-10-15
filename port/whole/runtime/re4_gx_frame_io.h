// Chunks of the emulated graphics processor (re4_gx_frame.h) as files: the
// local check with disc data dumps chosen frames (RE4_GX_DUMP, core_api.cpp)
// and the app's host tool gx_chunk_render draws them with the app's GX
// renderer. A dumped chunk starts over (RE4_GX_CHUNK_RESET): it holds the
// whole register state and every texture it uses.
//
// Layout (little-endian, as both sides run on little-endian hosts): magic
// "RE4GXCH1", then the 32-bit header words below, then ops (opWords * 4
// bytes), vertices, indices (indexCount * 2 bytes), blob.
#ifndef RE4_GX_FRAME_IO_H
#define RE4_GX_FRAME_IO_H

#include "re4_gx_frame.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
#ifndef RE4_GX_FRAME_IO_WRITE_ONLY
#include <vector>
#endif

namespace re4gxio {

static const char kMagic[8] = {'R', 'E', '4', 'G', 'X', 'C', 'H', '1'};

inline bool Write(FILE* f, const Re4GxChunk& c, uint32_t frame)
{
    const uint32_t head[14] = {c.version,   c.sequence,  c.flags,    c.opWords,   c.vertexBytes,
                               c.indexCount, c.blobBytes, c.xfbAddress, c.xfbWidth, c.xfbHeight,
                               c.draws,     c.vertexCount, c.textureBytes, frame};
    return fwrite(kMagic, 8, 1, f) == 1 && fwrite(head, sizeof(head), 1, f) == 1 &&
           (c.opWords == 0 || fwrite(c.ops, 4, c.opWords, f) == c.opWords) &&
           (c.vertexBytes == 0 || fwrite(c.vertices, 1, c.vertexBytes, f) == c.vertexBytes) &&
           (c.indexCount == 0 || fwrite(c.indices, 2, c.indexCount, f) == c.indexCount) &&
           (c.blobBytes == 0 || fwrite(c.blob, 1, c.blobBytes, f) == c.blobBytes);
}

#ifndef RE4_GX_FRAME_IO_WRITE_ONLY
// A chunk read back; `chunk` points into the vectors.
struct Loaded {
    std::vector<uint32_t> ops;
    std::vector<uint8_t> vertices;
    std::vector<uint16_t> indices;
    std::vector<uint8_t> blob;
    Re4GxChunk chunk;
    uint32_t frame;
};

inline bool Read(FILE* f, Loaded& out)
{
    char magic[8];
    uint32_t head[14];
    if (fread(magic, 8, 1, f) != 1 || memcmp(magic, kMagic, 8) != 0 || fread(head, sizeof(head), 1, f) != 1) {
        return false;
    }
    memset(&out.chunk, 0, sizeof(out.chunk));
    out.chunk.version = head[0];
    out.chunk.sequence = head[1];
    out.chunk.flags = head[2];
    out.ops.resize(head[3]);
    out.vertices.resize(head[4]);
    out.indices.resize(head[5]);
    out.blob.resize(head[6]);
    out.chunk.xfbAddress = head[7];
    out.chunk.xfbWidth = head[8];
    out.chunk.xfbHeight = head[9];
    out.chunk.draws = head[10];
    out.chunk.vertexCount = head[11];
    out.chunk.textureBytes = head[12];
    out.frame = head[13];
    if ((!out.ops.empty() && fread(out.ops.data(), 4, out.ops.size(), f) != out.ops.size()) ||
        (!out.vertices.empty() && fread(out.vertices.data(), 1, out.vertices.size(), f) != out.vertices.size()) ||
        (!out.indices.empty() && fread(out.indices.data(), 2, out.indices.size(), f) != out.indices.size()) ||
        (!out.blob.empty() && fread(out.blob.data(), 1, out.blob.size(), f) != out.blob.size())) {
        return false;
    }
    out.chunk.ops = out.ops.data();
    out.chunk.opWords = (uint32_t) out.ops.size();
    out.chunk.vertices = out.vertices.data();
    out.chunk.vertexBytes = (uint32_t) out.vertices.size();
    out.chunk.indices = out.indices.data();
    out.chunk.indexCount = (uint32_t) out.indices.size();
    out.chunk.blob = out.blob.data();
    out.chunk.blobBytes = (uint32_t) out.blob.size();
    return true;
}
#endif

}  // namespace re4gxio
#endif

#endif
