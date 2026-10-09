// Native translation of src/game/yz2asm.cpp's PPC integer decoder, checked
// against tools/motion/yz2.py. This replaces hardware instructions, not gameplay.
#include "yz2_internal.h"

namespace re4::port::detail {
namespace {
u32 word(const u8* p) { u32 value; std::memcpy(&value, p, sizeof(value)); return value; }
void word(u8* p, u32 value) { std::memcpy(p, &value, sizeof(value)); }
void rebuildLookup(Yz2Model& model, Yz2InEv& ev) {
    auto& f = model.fd;
    u32 end = 0;
    for (int symbol = 0; symbol < f.n; ++symbol) {
        const u32 start = f.cum[symbol * 2 + 1];
        const u32 next = start + f.cum[symbol * 2];
        if (start != end || next > 0x8000) throw std::runtime_error("Invalid Yz2 cumulative model");
        for (u32 i = start; i < next; ++i) {
            if ((i & 255u) == 0) yz2Pulse(ev, 256);
            word(model.table + i * 4, static_cast<u32>(symbol));
        }
        end = next;
    }
    if (end != 0x8000) throw std::runtime_error("Incomplete Yz2 cumulative model");
    f.acc = 1; // Native cache-valid bit, replaces the assembly's FP flag.
}
u32 symbol(Yz2Model& model, Yz2InEv& ev) {
    yz2Pulse(ev, 1);
    auto& d = *model.dec;
    if (d.mask <= 0x800000) {
        const int count = d.mask > 0x8000 ? 1 : d.mask > 0x80 ? 2 : 3;
        for (int i = 0; i < count; ++i) {
            d.byte = (d.byte << 8) | yz2ReadByte(ev);
            d.mask <<= 8;
        }
    }
    const auto step = d.mask >> 14;
    if (!step) throw std::runtime_error("Invalid Yz2 arithmetic range");
    const auto value = d.byte / step;
    if (value >= 0x8000) throw std::runtime_error("Yz2 code is outside the cumulative table");
    auto& f = model.fd;
    if (!f.acc) rebuildLookup(model, ev);
    const auto s = word(model.table + value * 4);
    if (s >= static_cast<u32>(f.n)) throw std::runtime_error("Invalid Yz2 symbol");
    d.byte -= step * f.cum[s * 2 + 1];
    d.mask = (step * f.cum[s * 2]) >> 1; // PPC mullw then srwi: u32 arithmetic.
    ++f.freq[s];
    ++f.max;
    ++ev.statistics->symbols;
    if (f.bits <= 14) {
        if (f.max == f.range) {
            const auto shift = 15 - f.bits;
            u32 sum = 0;
            for (int i = 0; i < f.n; ++i) {
                yz2Pulse(ev, 1);
                const u32 v = static_cast<u32>(f.freq[i]) << shift;
                f.cum[i * 2] = static_cast<u16>(v);
                f.cum[i * 2 + 1] = static_cast<u16>(sum);
                sum += v;
            }
            ++f.bits;
            f.range = 1u << f.bits;
            f.acc = 0;
        }
    } else if (f.max > 0x7FFF) {
        u32 sum = 0, total = 0;
        for (int i = 0; i < f.n; ++i) {
            yz2Pulse(ev, 1);
            u16 v = f.freq[i];
            // Original builds cum BEFORE halving the running frequencies.
            f.cum[i * 2] = v;
            f.cum[i * 2 + 1] = static_cast<u16>(sum);
            sum += v;
            if (v > 1) { v >>= 1; f.freq[i] = v; }
            total += v;
        }
        f.max = total;
        f.acc = 0;
        if (&model == &d.m1) ++ev.statistics->mainRescales;
        else ++ev.statistics->sideRescales;
    }
    return s;
}
}
void yz2Decode_Decode(void* context, void* output, u32 bytes, void* input) {
    auto& ctx = *static_cast<Yz2Ctx*>(context);
    auto& ev = *static_cast<Yz2InEv*>(input);
    auto* dst = static_cast<u8*>(output);
    u32 o = 0, scan = 0;
    while (o < bytes) {
        const auto s = symbol(ctx.d.m1, ev);
        u32 length = 1;
        if (s > 0x3FF) { dst[o++] = static_cast<u8>(s); }
        else {
            if (!o || scan >= o) throw std::runtime_error("Yz2 match has no preceding context");
            auto& entry = ctx.dic[dst[scan]];
            const auto slot = (s + entry.cnt) & 0x1FFu;
            // PPC stored 32-bit output pointers. Native dictionaries instead
            // store 32-bit output OFFSETS; zero is never a valid recorded source.
            const u32 source = word(entry.a + slot * 4);
            if (s > 0x1FF) {
                length = word(entry.b + slot * 4);
                ++ev.statistics->storedLengths;
            } else {
                const auto t = symbol(ctx.d.m2, ev);
                if (t > 2) length = t;
                else {
                    const int count = 4 - static_cast<int>(t);
                    length = 0;
                    for (int i = 0; i < count; ++i) length = (length << 8) | symbol(ctx.d.m2, ev);
                    if (t == 2) ++ev.statistics->length16;
                    else if (t == 1) ++ev.statistics->length24;
                    else ++ev.statistics->length32;
                }
                --length;
            }
            if (!source || source >= o || !length || length > bytes - o)
                throw std::runtime_error("Yz2 dictionary reference/length exceeds output bounds");
            // Overlapping byte-copy semantics, never memcpy/memmove.
            for (u32 k = 0; k < length; ++k) {
                if ((k & 255u) == 0) yz2Pulse(ev, 256);
                dst[o + k] = dst[source + k];
            }
            o += length;
            ++ev.statistics->copiedRuns;
        }
        const auto last = o - 1;
        if (scan < last) {
            auto& entry = ctx.dic[dst[scan]];
            word(entry.a + entry.cnt * 4, scan + 1);
            word(entry.b + entry.cnt * 4, length);
            entry.cnt = (entry.cnt + 1) & 0x1FF;
            if (!entry.cnt) ++ev.statistics->dictionaryWraps;
            scan = last;
        }
    }
}
} // namespace re4::port::detail
