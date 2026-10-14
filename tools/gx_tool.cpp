#include "archive.h"
#include "smd.h"
#include <iostream>
#include <stdexcept>
void word(std::uint32_t v) {
  for (int i = 3; i >= 0; --i)
    std::cout.put(static_cast<char>(v >> (8 * i)));
}
int main(int argc, char **argv) {
  try {
    if (argc != 2)
      return 2;
    auto arc = re4::RoomArchive::Load(argv[1]);
    for (int occurrence = 0; const auto *e = arc.find("SMD", occurrence);
         ++occurrence) {
      auto bytes = arc.copy(static_cast<std::size_t>(e - arc.entries().data()));
      if (bytes.size() < 16)
        throw std::runtime_error("Short SMD");
      if (bytes[0] != 0x40)
        continue;
      re4::Smd s(bytes);
      for (std::size_t i = 0; i < s.models.size(); ++i) {
        const auto &m = s.models[i];
        word(occurrence);
        word(i);
        word(m.primitives.size());
        word(m.materialHeaders.size());
        for (const auto &h : m.materialHeaders)
          for (auto b : h)
            std::cout.put(static_cast<char>(b));
        for (const auto &p : m.primitives) {
          word(p.displayList);
          word(p.opcode);
          word(p.vertices.size());
          word(p.triangles.size());
          for (const auto &v : p.vertices) {
            word(v.position);
            word(v.normal);
            word(v.color);
            word(v.texcoord);
          }
          for (const auto &t : p.triangles)
            for (auto v : t)
              word(v);
        }
      }
    }
    if (!std::cout)
      throw std::runtime_error("Output error");
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
