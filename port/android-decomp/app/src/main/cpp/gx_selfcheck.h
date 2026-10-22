// The GPU checks itself once per run: does it compute what the generated TEV shaders say?
#pragma once

#include <string>

namespace re4gx {

// The colours of the game's movies (game/sofdec.cpp setTevPrm: Y, U and V through four TEV stages
// with a negative colour register and a doubling scale) for eight test colours, after 1, 2, 3 and 4
// stages, on a renderer of its own at scale 1 (the GL state is left at the defaults). Returns the
// log line; `wrong` gets the number of wrong pixels (0: the GPU computes them like the GameCube).
// A Galaxy A36 (Adreno 710) showed the movies green and brown (tester, 22.10.2026).
std::string MovieColourCheck(int *wrong);

} // namespace re4gx
