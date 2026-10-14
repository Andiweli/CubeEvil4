// re4_format_compare, app side: the checked big-endian readers of
// RE4-Android-Decomp (port/android-decomp/app/src/main/cpp).
#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace re4cmp {

using Bytes = std::vector<std::uint8_t>;
using FactList = std::vector<std::pair<std::string, std::string>>;

// Each reports what the app's readers see, with the keys of game_side.cpp.
// A part the app's reader rejects becomes a "<part>skip" fact with the
// reader's message; the comparison leaves that part out on both sides.
// Throws when the app rejects the file as a whole.
void appArchive(const Bytes& bytes, FactList& out);
void appEsl(const Bytes& bytes, FactList& out);
void appEvd(const Bytes& bytes, FactList& out);
void appRoomInfo(const Bytes& bytes, FactList& out);

// The app's Yz2 decoder (ports/yz2.cpp): a room file (.das) to its archive.
Bytes appUnpackYz2(const Bytes& source);

}  // namespace re4cmp
